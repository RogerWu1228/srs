#include "worker.h"
#include "../json/json.hpp"
#include "../common/tunnel_proto.h"
#include "../log/LogWrapper.h"

extern LogWrapper g_log;
using json = nlohmann::json;
Worker::Worker()
	: _received_quick_packets(1000), _received_tcp_packets(1000)
{
	ParseJson();
	_quick = Quick::Create(_local_udp_port, false, this);
	_session_id = _quick->ConnectToServer(_server_ip, _server_port);
	_tcp_server.reset(new TcpServer(_listen_tcp_port, this));
	OutputInfo("init quick client, port = %d, quick session id = %u", _local_udp_port, _session_id);
}

Worker::~Worker()
{
	_tcp_server->Stop();
	_tcp_server.reset();
}

extern uint64_t g_print_send_tick;
extern uint64_t g_total_send_bytes;
void Worker::Exec()
{
	while (_running) {
		auto now = GetCurrentTick();
		if (now - g_print_send_tick >= 1000) {
			g_print_send_tick = now;
			OutputInfo("tcp total send: %llu bytes", g_total_send_bytes);
		}
		DeliverQuickPackets();
		DeliverTcpPackets();
		MaybeSleep();
	}
}

void Worker::OnTcpNewConnection(TSOCKET s)
{
	OutputInfo("new tcp client connected, socket = %d", s);
	while (1) {
		auto p = _received_tcp_packets.GetWriteablePacket();
		if (p == nullptr) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		p->connected = true;
		p->socket = s;
		_received_tcp_packets.WritePacket();
		break;
	}
	MaybeWakeup();
}

void Worker::OnTcpDisconnection(TSOCKET s)
{
	OutputInfo("tcp client disconnected, socket = %d", s);
	while (1) {
		auto p = _received_tcp_packets.GetWriteablePacket();
		if (p == nullptr) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		p->disconnected= true;
		p->socket = s;
		_received_tcp_packets.WritePacket();
		break;
	}
	MaybeWakeup();
}

void Worker::OnTcpMessage(TSOCKET s, uint8_t* data, uint16_t data_len)
{
	while (1) {
		auto p = _received_tcp_packets.GetWriteablePacket();
		if (p == nullptr) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		p->Write(s, data, data_len);
		_received_tcp_packets.WritePacket();
		break;
	}
	MaybeWakeup();
}

void Worker::OnConnected(uint32_t session_id)
{
	_quick_server_state = true;
	OutputInfo("quick server connected");
	AutoLock lock(_lock);
	for (auto iter : _tcp_connections) {
		iter.second->SetQuickServerState(true);
		iter.second->SendTunnelConnectMsg();
	}
}

void Worker::OnDisconnected(uint32_t session_id)
{
	_quick_server_state = false;
	OutputInfo("quick server disconnected, session_id = %u", session_id);
	AutoLock lock(_lock);
	// 最好还是删除所有的连接
	for (auto iter : _tcp_connections) {
		iter.second->SetQuickServerState(false);
	}
}

void Worker::OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len)
{
	while (1) {
		auto p = _received_quick_packets.GetWriteablePacket();
		if (p == nullptr) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		p->Write(session_id, data, data_len);
		_received_quick_packets.WritePacket();
		break;
	}
	MaybeWakeup();
}

void Worker::OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port)
{

}

void Worker::ParseJson()
{
	auto file_name = GetCurrentDir() + "quick_tunnel_client.json";
	FILE *file = fopen(file_name.c_str(), "rb");
	if (file == nullptr)
		return;
	int buf_len = 1024 * 1024;
	std::unique_ptr<uint8_t> buf(new uint8_t[buf_len]);
	memset(buf.get(), 0, buf_len);
	int read_len = fread(buf.get(), 1, buf_len, file);
	fclose(file);
	if (read_len <= 0)
		return;
	json root = json::parse(buf.get());
	auto iter = root.find("listen_tcp_port");
	if (iter != root.end())
		_listen_tcp_port = iter.value().get<int>();
	iter = root.find("local_udp_port");
	if (iter != root.end())
		_local_udp_port = iter.value().get<int>();
	iter = root.find("server_ip");
	if (iter != root.end())
		_server_ip = iter.value().get<std::string>();
	iter = root.find("server_port");
	if (iter != root.end())
		_server_port = iter.value().get<int>();
}

void Worker::DeliverTcpPackets()
{
	while (true) {
		auto p = _received_tcp_packets.GetReadablepacket();
		if (p == nullptr)
			break;
		{
			AutoLock lock(_lock);
			if (p->connected) {
				Connection *c = new Connection(_quick.get(), p->socket, p->socket, _session_id, true, _quick_server_state);
				_tcp_connections[p->socket] = c;
				p->connected = false;
				g_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "socket %u is from connected, put it to tcp_connection", p->socket);
			}
			else if (p->disconnected) {
				p->disconnected = false;
				auto iter = _tcp_connections.find(p->socket);
				if (iter != _tcp_connections.end()) {
					g_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "socket %u is disconnect, remove from tcp_connection", p->socket);
					iter->second->TcpClose();
					iter->second->SendTunnelDisconnectMsg();
					delete iter->second;
					_tcp_connections.erase(iter);
				}
				else
					g_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, " remove socket %u from tcp_connection failed", p->socket);
			}
			else {
				auto iter = _tcp_connections.find(p->socket);
				if (iter != _tcp_connections.end())
					iter->second->DeliverTcpPacket(p->data.get(), p->data_len);
				else
					int a = 0;
			}
		}
		_received_tcp_packets.RemovePacket();
	}

	AutoLock lock(_lock);
	for (auto iter : _tcp_connections) {
		iter.second->DeliverTcpPackets();
	}
}

void Worker::DeliverQuickPackets()
{
	while (true) {
		auto p = _received_quick_packets.GetReadablepacket();
		if (p == nullptr)
			break;;
		{
			uint32_t socket = 0;
			uint8_t msg_id = 0;
			TunnelConnect* msg = (TunnelConnect*)p->data.get();
			AutoLock lock(_lock);
			static uint8_t head_len = sizeof(uint8_t) + sizeof(uint32_t);
			auto iter = _tcp_connections.find(msg->socket);
			if (iter != _tcp_connections.end()) {
				if (msg->msg_id == TUNNEL_MSG_PAYLOAD)
					iter->second->DeliverQuickPacket(p->data.get() + head_len, p->data_len - head_len);
				else if (msg->msg_id == TUNNEL_MSG_DISCONNECT) {
					iter->second->QuickClose();
					TSOCKET s = iter->first;
					OutputInfo("connection close by tunnel server , socket = %u", s);
					delete iter->second;
					_tcp_connections.erase(iter);
					_lock.LockEnd();
					_tcp_server->Disconnect(s);
				}
			}
			else
				int a = 0;
		}
		_received_quick_packets.RemovePacket();
	}

	AutoLock lock(_lock);
	for (auto iter : _tcp_connections) {
		if (!iter.second->DeliverQuickPackets()) {
			OutputInfo("DeliverQuickPackets failed, socket = %u", iter.first);
		}
	}
}

void Worker::MaybeSleep()
{
	if (_received_quick_packets.GetReadablepacket() || _received_tcp_packets.GetReadablepacket())
		return;
	{
		AutoLock lock(_lock);
		for (auto c : _tcp_connections) {
			if (!c.second->CanSleep())
				return;
		}
	}
	_sleeping = true;
	_event.Wait(1);
}

void Worker::MaybeWakeup()
{
	_event.Set();
}
