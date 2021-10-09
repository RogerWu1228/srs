#pragma once
#include "../common/tcp_base.h"
#include <string>
#include <unordered_map>
#include "../common/utility.h"
#include <thread>

using namespace dd;

class TcpClientCallback {
public:
	virtual void OnTcpConnected(TSOCKET s) = 0;
	virtual void OnTcpDisconnected(TSOCKET s) = 0;
	virtual void OnTcpMessage(TSOCKET s, const uint8_t* data, uint16_t data_len) = 0;
};

class TcpClient : public TcpBase
{
public:
	TcpClient(TcpClientCallback* cb, std::string &server_ip, uint16_t server_port);
	~TcpClient();

	TSOCKET CreateNewClient();
	void Connect(TSOCKET s);
	void CloseClient(TSOCKET s);

private:
	void Process();
	void TryConnect();
	TSOCKET InitFdset(fd_set& readset, fd_set& writeset);
	void HandleReadSet(fd_set& readset);
	void HandleWriteSet(fd_set& writeset);
	bool ConnectToServer(TSOCKET s);

	struct Client {
		bool try_connect = false;
		bool server_state = false;
	};
	std::string _server_ip;
	uint16_t _server_port;
	std::unordered_map<TSOCKET, Client> _clients;
	Lock _lock;
	std::unique_ptr<std::thread> _thread;
	bool _running = true;
	TcpClientCallback *_cb;

	uint64_t _print_tick = 0;
	uint64_t _recv_speed = 0;
};

