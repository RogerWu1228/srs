#include "connection.h"
#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#endif
#include "../common/utility.h"
#include "../common/tunnel_proto.h"
#include <thread>
#include "../log/LogWrapper.h"

using namespace dd;

uint64_t g_print_send_tick = 0;
uint64_t g_total_send_bytes = 0;
extern LogWrapper g_log;

Connection::Connection(Quick* quick, TSOCKET socket, TSOCKET client_socket, uint32_t session_id, bool connected, bool quick_server_state)
	: _quick(quick), _socket(socket), _client_socket(client_socket),_session_id(session_id), _connected(connected), _quick_server_state(quick_server_state),
	_need_send_tunnel_connect_msg(connected)
{
	_fix_head_len = sizeof(uint8_t) + sizeof(uint32_t);
	SendTunnelConnectMsg();
}

Connection::~Connection()
{
}

bool Connection::DeliverTcpPackets()
{
	if (!_quick_server_state)
		return true;
	uint32_t data_size = _received_tcp_data.data_size();
	uint8_t *data = _received_tcp_data.data();
	uint32_t total_sent = 0;
	uint16_t pacing_size = MAX_QUICK_PACKET_SIZE;
	uint32_t rest = data_size;
	bool ret = false;
	while (total_sent < data_size) {
		if (rest >= MAX_QUICK_PACKET_SIZE - _fix_head_len)
			pacing_size = MAX_QUICK_PACKET_SIZE - _fix_head_len;
		else
			pacing_size = rest;
		TunnelPayload *msg = (TunnelPayload*)_buf;
		msg->msg_id = TUNNEL_MSG_PAYLOAD;
		msg->socket = _client_socket;
		memcpy(msg->payload, data + total_sent, pacing_size);
		if (_quick->Send(_session_id, _buf, pacing_size + _fix_head_len)) {
			total_sent += pacing_size;
			rest -= pacing_size;
			_total_quick_sent_bytes += pacing_size;
		}
		else
			break;
	}
	_received_tcp_data.Remove(total_sent);
	TryDrainTcpPackets();
	return true;
}

bool Connection::DeliverQuickPackets()
{
	uint32_t data_size = _received_quick_data.data_size();
	if (!data_size || !_connected)
		return true;
	// test
//	int32_t real_send = data_size;

	int32_t real_send = send(_socket, (char*)_received_quick_data.data(), data_size, 0);
	if (real_send == -1) {
		int code = MyGetError();
#ifdef _WIN32
		if (code == WSAEWOULDBLOCK)
#else
		if (code == EINTR || code == EWOULDBLOCK || code == EAGAIN)
#endif
		{
			return true; 
		}
		else
			return false;// send error, close connection
	}
	_total_tcp_sent_bytes += real_send;
	_received_quick_data.Remove(real_send);
	g_total_send_bytes += real_send;

	//g_log.WriteHex(RTC_FROM_HERE, LOG_LEVEL_INFO, (char*)_received_quick_data.data(), real_send, "socket %u send %u bytes: ", _socket, real_send);
	TryDrainQuickPackets();
	return true;
}

void Connection::DeliverTcpPacket(const uint8_t *data, uint16_t data_len)
{
	_total_tcp_received_bytes += data_len;
	if (_not_deliver_tcp_packets.size() || !_received_tcp_data.CanAppend(data_len)) {
		TcpPacket p;
		p.Write(_socket, data, data_len);
		_not_deliver_tcp_packets.push_back(p);
	}
	else 
		_received_tcp_data.Append(data, data_len);
}

void Connection::DeliverQuickPacket(const uint8_t* data, uint16_t data_len)
{
	_total_quick_received_bytes += data_len;
	if (_not_deliver_quick_packets.size() || !_received_quick_data.CanAppend(data_len)) {
		QPacket p;
		p.Write(_session_id, data, data_len);
		_not_deliver_quick_packets.push_back(p);
	}
	else
		_received_quick_data.Append(data, data_len);
}

void Connection::TcpClose()
{
	if (!_quick_server_state)
		return;
	// 尝试发送所有的tcp数据
	int try_count = 10;
	while (try_count >= 0) {
		try_count--;
		DeliverTcpPackets();
		if(_received_tcp_data.data_size() == 0)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	OutputConnectionStatus();
}

void Connection::QuickClose()
{
	if (!_connected)
		return;
	// 尝试发送所有的quick数据
	int try_count = 10;
	while (try_count >= 0) {
		try_count--;
		DeliverQuickPackets();
		if (_received_quick_data.data_size() == 0)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	OutputConnectionStatus();
}

void Connection::SendTunnelConnectMsg()
{
	if (!_quick_server_state || _send_connect_msg || !_need_send_tunnel_connect_msg)
		return;
	_send_connect_msg = true;
	TunnelConnect msg;
	msg.socket = _client_socket;
	bool ret = _quick->Send(_session_id, (uint8_t*)&msg, sizeof(msg));
	if (!ret) {
		ret = ret;
	}
	OutputDebugInfo("Send connect msg, session_id = %u, socket = %u, client_socket = %u", _session_id, _socket, _client_socket);
}

bool Connection::SendTunnelDisconnectMsg()
{
	TunnelDisconnect msg;
	msg.socket = _client_socket;
	int count = 10;
	bool ret = false;
	while (count >= 0) {
		count--;
		ret = _quick->Send(_session_id, (uint8_t*)&msg, sizeof(msg));
		if (ret)
			break;
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	OutputDebugInfo("Send disconnect msg, ret = %d, session_id = %u, socket = %u, client_socket = %u", 
		ret, _session_id, _socket, _client_socket);
	return ret;
}

bool Connection::CanSleep()
{
	if (_received_quick_data.data_size() == 0 && _received_tcp_data.data_size() == 0 &&
		_not_deliver_quick_packets.size() == 0 && _not_deliver_tcp_packets.size() == 0)
		return true;
	return false;
}

void Connection::OutputConnectionStatus()
{
	OutputInfo("quick session id = %llu, socket = %u, client_socket = %u, tcp received: %llu, sent: %llu, quick received: %llu, sent: %llu",
		_session_id, _socket, _client_socket, _total_tcp_received_bytes, _total_tcp_sent_bytes, _total_quick_received_bytes, _total_quick_sent_bytes);
}

void Connection::TryDrainTcpPackets()
{
	for (auto iter = _not_deliver_tcp_packets.begin(); iter != _not_deliver_tcp_packets.end();) {
		if (_received_tcp_data.CanAppend(iter->data_len)) {
			_received_tcp_data.Append(iter->data.get(), iter->data_len);
			iter = _not_deliver_tcp_packets.erase(iter);
		}
		else
			break;
	}
}

void Connection::TryDrainQuickPackets()
{
	for (auto iter = _not_deliver_quick_packets.begin(); iter != _not_deliver_quick_packets.end();) {
		if (_received_quick_data.CanAppend(iter->data_len)) {
			_received_quick_data.Append(iter->data.get(), iter->data_len);
			iter = _not_deliver_quick_packets.erase(iter);
		}
		else
			break;
	}
}
