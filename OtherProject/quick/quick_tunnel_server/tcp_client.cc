#include "tcp_client.h"
#include "../log/LogWrapper.h"

extern LogWrapper g_log;
TcpClient::TcpClient(TcpClientCallback*cb, std::string &server_ip, uint16_t server_port)
	: _cb(cb), _server_ip(server_ip), _server_port(server_port)
{
	_thread.reset(new std::thread(&TcpClient::Process, this));
}

TcpClient::~TcpClient()
{
	_thread->join();
}

TSOCKET TcpClient::CreateNewClient()
{
	TSOCKET s = CreateSocket();
	return s;
}

void TcpClient::Connect(TSOCKET s)
{
	AutoLock lock(_lock);
	Client c;
	_clients[s] = c;
}

void TcpClient::CloseClient(TSOCKET s)
{
	{
		AutoLock lock(_lock);
		_clients.erase(s);
	}
	CloseSocket(s);
}

static uint64_t g_print_recv_tick = 0;
static uint64_t g_total_recv_bytes = 0;

void TcpClient::Process()
{
	while (_running) {
		auto now = GetCurrentTick();
		if (now - g_print_recv_tick >= 1000) {
			g_print_recv_tick = now;
			OutputInfo("tcp total recv: %llu bytes", g_total_recv_bytes);
		}
		if (now - _print_tick >= 1000) {
			_print_tick = now;
			float speed = (float)_recv_speed / 1024.0 / 1024.0;
			//OutputInfo("recv speed: %fMB/s", speed);
			_recv_speed = 0;
		}
		{
			AutoLock lock(_lock);
			if (_clients.size() == 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;;
			}
		}
		TryConnect();

		timeval time;
		time.tv_sec = 0;
		time.tv_usec = 1000 * 50;
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

void TcpClient::TryConnect()
{
	AutoLock lock(_lock);
	for (auto &c : _clients) {
		if(c.second.server_state)
			continue;
		c.second.server_state = ConnectToServer(c.first);
		c.second.try_connect = true;

		if (c.second.server_state)
			_cb->OnTcpConnected(c.first);
	}
}

TSOCKET TcpClient::InitFdset(fd_set& readset, fd_set& writeset)
{
	TSOCKET ret;
	FD_ZERO(&readset);
	FD_ZERO(&writeset);
	ret = -1;
	{
		AutoLock lock(_lock);
		if (_clients.size() == 0)
			return ret;
		for (auto &iter : _clients) {
			FD_SET(iter.first, &readset);
			FD_SET(iter.first, &writeset);
			if (iter.first > ret)
				ret = iter.first;
		}
	}
	return ret + 1;
}

void TcpClient::HandleReadSet(fd_set& readset)
{
	AutoLock lock(_lock);
	for (auto iter : _clients) {
		if (FD_ISSET(iter.first, &readset) == 0)
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
			if (code == ENOTCONN || code == EAGAIN || code == EINPROGRESS)
				continue;
#endif
			OutputInfo("recv failed, recv len = %d, code = %d, close connection", read_len, code);
			CloseSocket(iter.first);
			_cb->OnTcpDisconnected(iter.first);
			_clients.erase(iter.first);
			break;
		}
		else {
			_recv_speed += read_len;
			g_total_recv_bytes += read_len;

			// test
			//return;
		/*	static int index = 0;
			if (index > 500)
				return;
			index++;*/
			//g_log.WriteHex(RTC_FROM_HERE, LOG_LEVEL_INFO, buf, read_len, "socket %u recv %u bytes: ", iter.first, read_len);

			_cb->OnTcpMessage(iter.first, (uint8_t*)buf, read_len);
		}
	}
}

void TcpClient::HandleWriteSet(fd_set& writeset)
{
	AutoLock lock(_lock);
	for (auto iter : _clients) {
		if (FD_ISSET(iter.first, &writeset) == 0)
			continue;
		if(iter.second.server_state)
			continue;

		iter.second.server_state = true;
		_cb->OnTcpConnected(iter.first);
	}
}

bool TcpClient::ConnectToServer(TSOCKET s)
{
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(_server_ip.c_str());
	addr.sin_port = htons(_server_port);
	int ret = connect(s, (sockaddr*)&addr, sizeof(addr));
	bool state = false;
	if (ret == -1) {
		auto code = MyGetError();
#ifdef _WIN32
		if (code == WSAEISCONN)
			state = true;
#else
		if (code == EISCONN)
			state = true;
#endif
	}
	else {
		state = true;
	}
	return state;
}
