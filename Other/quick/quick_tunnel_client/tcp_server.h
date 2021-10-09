#pragma once
#include "../common/tcp_base.h"
#include <unordered_map>
#include "../common/utility.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <list>

using namespace dd;
class ServerCallback {
public:
	virtual void OnTcpNewConnection(TSOCKET s) = 0;
	virtual void OnTcpDisconnection(TSOCKET s) = 0;
	virtual void OnTcpMessage(TSOCKET s, uint8_t* data, uint16_t data_len) = 0;
};

struct Packet {
	uint16_t data_len = 0;
	std::unique_ptr<uint8_t> data;
	Packet() {}
	Packet(const uint8_t* data, uint16_t data_len) {
		this->data_len = data_len;
		this->data.reset(new uint8_t[data_len]);
		memcpy(this->data.get(), data, data_len);
	}
	Packet(const Packet& a) {
		this->data_len = a.data_len;
		this->data.reset(new uint8_t[a.data_len]);
		memcpy(this->data.get(), a.data.get(), data_len);
	}
	Packet& operator=(const Packet& a){
		this->data_len = a.data_len;
		this->data.reset(new uint8_t[a.data_len]);
		memcpy(this->data.get(), a.data.get(), data_len);
		return *this;
	}
};
class TcpServer : public TcpBase
{
public:
	TcpServer(uint16_t port, ServerCallback* cb);
	~TcpServer();
	void Send(TSOCKET s, const uint8_t* data, uint16_t data_len);
	void Disconnect(TSOCKET s);
	void Stop() { _running = false; }

private:
	void Process();
	void InitSocket();
	TSOCKET InitFdset(fd_set& readset, fd_set& writeset);
	void HandleReadSet(fd_set& readset);
	void HandleWriteSet(fd_set& writeset);
	void AcceptNewClient();
	

	struct Client {
		std::string ip;
		uint16_t port;
		std::list<Packet> wait_send_pkts;
		Client(const std::string &ip, uint16_t port) {
			this->ip = ip;
			this->port = port;
		}
		Client() {}
	};

	uint16_t _port;
	ServerCallback* _cb;
	std::unordered_map < TSOCKET, Client> _clients;
	Lock _lock;
	std::unique_ptr<std::thread> _thread;
	bool _running = true;
};

