#pragma once
#include <memory>
#ifdef _WIN32
#define TSOCKET uint64_t
#define MyGetError() GetLastError()
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <string.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define TSOCKET int
#define MyGetError() errno
#endif


class TcpBase
{
public:
	TcpBase();
	virtual ~TcpBase();

	TSOCKET CreateSocket();
	void CloseSocket(TSOCKET s);
	void SetSocketNoblock(TSOCKET s);
	int Send(TSOCKET s, const uint8_t* data, uint16_t data_len);

protected:
	TSOCKET _socket;
private:
	void Init();
	void Uninit();
};

