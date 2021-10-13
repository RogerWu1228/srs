#pragma once
#include <memory>
#include <string>
#include <map>
#include "../quick/quick.h"
#include "tcp_client.h"
#include "../common/utility.h"
#include "../common/ring_buffer.h"
#include "../common/connection.h"
#include "../common/tunnel_proto.h"
#include "../thread/event.h"

class Worker : public QuickCallback, public TcpClientCallback
{
public:
	Worker();
	~Worker();

	void Exec();

	void OnTcpConnected(TSOCKET s) override;
	void OnTcpDisconnected(TSOCKET s) override;
	void OnTcpMessage(TSOCKET s, const uint8_t* data, uint16_t data_len) override;

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
	struct ConnectionId {
		uint32_t session_id;
		uint32_t socket;
		ConnectionId(uint32_t session_id, uint32_t socket) {
			this->session_id = session_id;
			this->socket = socket;
		}

		bool operator==(const ConnectionId& other)const {
			if (session_id == other.session_id && socket == other.socket)
				return true;
			return false;
		}

		bool operator<(const ConnectionId& other)const {
			if (session_id < other.session_id)
				return true;
			else {
				if (socket < other.socket)
					return true;
			}
			return false;
		}
	};

	bool _running = true;
	uint16_t _local_udp_port;
	std::string _server_ip;
	uint16_t _server_port;
	std::unique_ptr<Quick> _quick;
	std::unique_ptr<TcpClient> _tcp_client;

	RingBuffer<QPacket> _received_quick_packets;
	RingBuffer<TcpPacket> _received_tcp_packets;
	// _tcp_connections和_quick_connections里面的Connection是相同 的，只是为了方便查找，所以两个两个map来存储
	std::unordered_map<TSOCKET, Connection*> _tcp_connections;
	// _quick_connectoions不能使用socket作为key，因为不同的tunnel client可能会出现相同的socket
	std::map<ConnectionId, Connection*> _quick_connections;
	Lock _lock;

	dd::Event _event;
	bool _sleeping = false;
	uint64_t _remove_invalid_connection_time = 0;
};

