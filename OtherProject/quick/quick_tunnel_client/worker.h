#pragma once
#include "tcp_server.h"
#include "../quick/quick.h"
#include <unordered_map>
#include "../common/connection.h"
#include "../common/ring_buffer.h"
#include "../thread/event.h"

class Worker : public ServerCallback, public QuickCallback
{
public:
	Worker();
	~Worker();
	void Exec();

	void OnTcpNewConnection(TSOCKET s) override;
	void OnTcpDisconnection(TSOCKET s)override;
	void OnTcpMessage(TSOCKET s, uint8_t* data, uint16_t data_len)override;

	void OnConnected(uint32_t session_id) override;
	void OnDisconnected(uint32_t session_id)override;
	void OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len)override;
	void OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port);

private:
	void ParseJson();
	void DeliverTcpPackets();
	void DeliverQuickPackets();
	void MaybeSleep();
	void MaybeWakeup();

	std::unique_ptr<Quick> _quick;
	uint16_t _listen_tcp_port;
	uint16_t _local_udp_port;
	std::string _server_ip;
	uint16_t _server_port;
	std::unique_ptr<TcpServer> _tcp_server;
	bool _quick_server_state = false;
	bool _running = true;
	RingBuffer<QPacket> _received_quick_packets;
	RingBuffer<TcpPacket> _received_tcp_packets;

	std::unordered_map<TSOCKET, Connection*> _tcp_connections;

	Lock _lock;
	dd::Event _event;
	bool _sleeping = false;
	uint32_t _session_id = 0;
};

