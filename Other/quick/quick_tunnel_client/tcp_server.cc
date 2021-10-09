#include "tcp_server.h"
#include <assert.h>
#ifndef _WIN32
#define SOCKET_ERROR -1
#endif
#include "../log/LogWrapper.h"

extern LogWrapper g_log;

TcpServer::TcpServer(uint16_t port, ServerCallback* cb)
	: _port(port), _cb(cb)
{
	assert(_port != 0);
	InitSocket();
	_thread.reset(new std::thread(&TcpServer::Process, this));
}

TcpServer::~TcpServer() {
	_thread->join();
}

void TcpServer::Send(TSOCKET s, const uint8_t* data, uint16_t data_len)
{
	AutoLock lock(_lock);
	auto iter = _clients.find(s);
	if (iter == _clients.end())
		return;
	iter->second.wait_send_pkts.push_back(Packet(data, data_len));
}

void TcpServer::Disconnect(TSOCKET s)
{
	AutoLock lock(_lock);
	auto iter = _clients.find(s);
	if (iter == _clients.end())
		return;
	_clients.erase(iter);
	CloseSocket(s);
}

static uint64_t g_print_recv_tick = 0;
static uint64_t g_total_recv_bytes = 0;
void TcpServer::Process()
{
	while (_running) {
		auto now = GetCurrentTick();
		if (now - g_print_recv_tick >= 1000) {
			g_print_recv_tick = now;
			OutputInfo("tcp total recv: %llu bytes", g_total_recv_bytes);
		}
		timeval time;
		time.tv_sec = 0;
		time.tv_usec = 1000 * 2000;
		fd_set readset;
		fd_set writeset;
		TSOCKET fd;
		int ret = 0;
		fd = InitFdset(readset, writeset);
		ret = select(fd, &readset, nullptr, nullptr, &time);
		if (ret < 0)
			continue;
		HandleReadSet(readset);
		//HandleWriteSet(writeset);
	}
}

void TcpServer::InitSocket()
{
	_socket = CreateSocket();
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
#ifndef _WIN32
	int opt = 1;
	setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
	int ret = bind(_socket, (struct sockaddr*)&addr, sizeof(addr));
	if (ret == SOCKET_ERROR) {
		OutputInfo("socket bind port(%d) failed", _port);
		return;
	}
	ret = listen(_socket, SOMAXCONN);
	if (ret == SOCKET_ERROR) {
		OutputInfo("socket listen falied, error code = %d", MyGetError());
		return;
	}
	OutputInfo("tcp server init successed, listen port %d", _port);
}

TSOCKET TcpServer::InitFdset(fd_set& readset, fd_set& writeset)
{
	TSOCKET ret;
	FD_ZERO(&readset);
	FD_ZERO(&writeset);
	FD_SET(_socket, &readset);
	FD_SET(_socket, &writeset);
	ret = _socket;
	{
		AutoLock lock(_lock);
		for (auto &iter : _clients) {
			FD_SET(iter.first, &readset);
			FD_SET(iter.first, &writeset);
			if (iter.first > ret)
				ret = iter.first;
		}
	}
	return ret + 1;
}

void TcpServer::HandleReadSet(fd_set& readset)
{

	if (FD_ISSET(_socket, &readset))
		AcceptNewClient();
	AutoLock lock(_lock);
	for (auto iter : _clients) {
		if(FD_ISSET(iter.first, &readset) == 0)
			continue;
#define RECV_BUF_LEN 10240
		static char buf[RECV_BUF_LEN];
		int read_len = recv(iter.first, buf, RECV_BUF_LEN, 0);
		auto code = MyGetError();
#ifdef _WIN32
		if (code == WSAEWOULDBLOCK)
			continue;
#endif

		if (read_len <= 0) {
#ifndef _WIN32
			if(code == ENOTCONN || code == EAGAIN || code == EINPROGRESS)
				continue;
#endif
			OutputInfo("recv failed, recv len = %d, code = %d, close connection", read_len, code);
			CloseSocket(iter.first);
			_cb->OnTcpDisconnection(iter.first);
			_clients.erase(iter.first);
			break;
		}
		else {
			g_total_recv_bytes += read_len;
			_cb->OnTcpMessage(iter.first, (uint8_t*)buf, read_len);

			//g_log.WriteHex(RTC_FROM_HERE, LOG_LEVEL_INFO, (char*)buf, read_len, "socket %u recv %u bytes: ", iter.first, read_len);
		}
	}
}

void TcpServer::HandleWriteSet(fd_set& writeset)
{

}

void TcpServer::AcceptNewClient()
{
	sockaddr_in addr;
#ifdef _WIN32
	int addr_len = sizeof(addr);
#else
	socklen_t addr_len = sizeof(addr);
#endif
	TSOCKET socket = accept(_socket, (struct sockaddr*)&addr, &addr_len);
	if (socket == INVALID_SOCKET) {
		OutputInfo("accept failed, error code = %d", MyGetError());
		return;
	}

	std::string ip = inet_ntoa(addr.sin_addr);
	uint16_t port = ntohs(addr.sin_port);
	Client c(ip, port);
	{
		AutoLock lock(_lock);
		_clients[socket] = c;
	}
	_cb->OnTcpNewConnection(socket);
}
