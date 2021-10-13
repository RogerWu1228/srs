#include "tcp_base.h"
#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#endif
#include "../common/utility.h"
#include <thread>
#include <chrono>

using namespace dd;
TcpBase::TcpBase()
{
	Init();
}

TcpBase::~TcpBase()
{
	Uninit();
}

TSOCKET TcpBase::CreateSocket()
{
	TSOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		OutputInfo("create socket falied, error code = %d", MyGetError());
#ifdef _WIN32
		WSACleanup();
#endif
		return s;
	}
	//OutputInfo("create socket soccessed, socket = %d", s);
	SetSocketNoblock(s);
	return s;
}

void TcpBase::CloseSocket(TSOCKET s)
{
#ifdef WIN32
	closesocket(s);
#else
	close(s);
#endif;
}

void TcpBase::SetSocketNoblock(TSOCKET s)
{
#ifdef _WIN32
	unsigned long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);
#else
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
	int o = 0;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&o, sizeof(o));
#endif
}

int TcpBase::Send(TSOCKET s, const uint8_t* data, uint16_t data_len)
{
	uint16_t real_send = 0;
	int total_send = 0;
	do {
		real_send = send(s, (char*)data + total_send, data_len - total_send, 0);
		if (real_send == -1) {
			int code = MyGetError();
#ifdef _WIN32
			if(code == WSAEWOULDBLOCK)
#else
			if (code == EINTR || code == EWOULDBLOCK || code == EAGAIN)
#endif
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				if (total_send == 0)
					return -2;
				continue;
			}
			else
				return -1;
		}
		total_send += real_send;
	} while (total_send < data_len);
	return total_send;
}

void TcpBase::Init()
{
#ifdef _WIN32
	WSADATA data;
	int result = WSAStartup(MAKEWORD(2, 2), &data);
	if (result != NO_ERROR) {
		OutputInfo("WSAStartup failed, ret = %d", result);
		return;
	}
#endif
}

void TcpBase::Uninit()
{
	CloseSocket(_socket);
#ifdef _WIN32
	WSACleanup();
	OutputInfo("WSACleanup");
#endif
}
