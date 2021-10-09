#include "worker.h"
#include <thread>
#include <chrono>
#include "../json/json.hpp"
#include "../common/utility.h"

using json = nlohmann::json;
using namespace dd;

Worker::Worker()
	: _received_quick_packets(1000), _received_tcp_packets(1000)
{
	ParseJson();
	_quick = Quick::Create(_local_udp_port, true, this);
	_tcp_client.reset(new TcpClient(this, _server_ip, _server_port));
	OutputInfo("init quick server , local udp port = %d", _local_udp_port);
	OutputInfo("tcp server ip = %s, port = %d", _server_ip.c_str(), _server_port);
}

Worker::~Worker()
{

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
		DeliverTcpPackets();
		DeliverQuickPackets();
		MaybeSleep();
	}
}

void Worker::OnTcpConnected(TSOCKET s)
{
	OutputInfo("socket %u connected to server", s);
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

void Worker::OnTcpDisconnected(TSOCKET s)
{
	OutputInfo("tcp connection disconnected, socket = %u", s);
	while (1) {
		auto p = _received_tcp_packets.GetWriteablePacket();
		if (p == nullptr) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		p->disconnected = true;
		p->socket = s;
		_received_tcp_packets.WritePacket();
		break;
	}
	MaybeWakeup();
}

void Worker::OnTcpMessage(TSOCKET s, const uint8_t* data, uint16_t data_len)
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

}

void Worker::OnDisconnected(uint32_t session_id)
{
	OutputInfo("quick client disconnected, session_id = %u", session_id);
	AutoLock lock(_lock);
	for (auto iter = _quick_connections.begin(); iter != _quick_connections.end();) {
		if (iter->second->session_id() == session_id) {
			TSOCKET s = iter->second->socket();
			_tcp_connections.erase(s);
			delete iter->second;
			iter = _quick_connections.erase(iter);
			_tcp_client->CloseClient(s);
		}
		else {
			iter++;
		}
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
	OutputInfo("new quick client connected, session_id = %u, ip = %s, port = %d", session_id, ip.c_str(), port);
}

void Worker::ParseJson()
{
	auto file_name = GetCurrentDir() + "quick_tunnel_server.json";
	FILE *file = fopen(file_name.c_str(), "rb");
	if (file == nullptr) {
		OutputInfo("cannot open quick_tunnel_server.json");
		assert(0);
		return;
	}
	int buf_len = 1024 * 1024;
	std::unique_ptr<uint8_t> buf(new uint8_t[buf_len]);
	memset(buf.get(), 0, buf_len);
	int read_len = fread(buf.get(), 1, buf_len, file);
	fclose(file);
	if (read_len <= 0)
		return;
	json root = json::parse(buf.get());
	auto iter = root.find("local_udp_port");
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
			auto iter = _tcp_connections.find(p->socket);
			if (iter != _tcp_connections.end()) {
				if (p->connected)
					iter->second->SetConnected(true);
				else if (p->disconnected) {
					iter->second->TcpClose();
					iter->second->SendTunnelDisconnectMsg();
					TSOCKET client_socket = iter->second->client_socket();
					uint32_t session_id = iter->second->session_id();
					delete iter->second;
					_tcp_connections.erase(iter);
					_quick_connections.erase(ConnectionId(session_id, client_socket));
				}
				else
					iter->second->DeliverTcpPacket(p->data.get(), p->data_len);
			}
			else {
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
			TunnelConnect* msg = (TunnelConnect*)p->data.get();
			static uint8_t head_len = sizeof(uint8_t) + sizeof(uint32_t);
			AutoLock lock(_lock);
			if (msg->msg_id == TUNNEL_MSG_PAYLOAD) {
				auto iter = _quick_connections.find(ConnectionId(p->session_id, msg->socket));
				if (iter != _quick_connections.end())
					iter->second->DeliverQuickPacket(p->data.get() + head_len, p->data_len - head_len);
				else
					return;	// 未找到对应的connection,是因为connection还没有被插入到map里面，这种情况直接返回。
			}
			else if (msg->msg_id == TUNNEL_MSG_CONNECT) {
				TSOCKET s = _tcp_client->CreateNewClient();
				Connection *c = new Connection(_quick.get(), s, msg->socket, p->session_id, false, true);
				_tcp_connections[s] = c;
				_quick_connections[ConnectionId(p->session_id, msg->socket)] = c;
				_lock.LockEnd();
				_tcp_client->Connect(s);
				OutputInfo("recv tunnel client connect msg, create new socke, socket = %u", s);
			}
			else if (msg->msg_id == TUNNEL_MSG_DISCONNECT) {
				auto iter = _quick_connections.find(ConnectionId(p->session_id, msg->socket));
				if (iter != _quick_connections.end()) {
					iter->second->QuickClose();
					TSOCKET s = iter->second->socket();
					delete iter->second;
					_quick_connections.erase(iter);
					_tcp_connections.erase(s);
					_lock.LockEnd();
					_tcp_client->CloseClient(s);
					OutputDebugInfo("recv tunnel client disconnect msg, session_id = %u, client_socket = %u", p->session_id, msg->socket);
				}
			}
		}
		_received_quick_packets.RemovePacket();
	}

	AutoLock lock(_lock);
	for (auto iter : _quick_connections) {
		if (!iter.second->DeliverQuickPackets()) {
			OutputInfo("DeliverQuickPackets failed, close the connection");
		}
	}
}

void Worker::MaybeSleep()
{
	if (_received_quick_packets.GetReadablepacket() || _received_tcp_packets.GetReadablepacket())
		return;
	{
		AutoLock lock(_lock);
		for (auto c : _quick_connections) {
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
