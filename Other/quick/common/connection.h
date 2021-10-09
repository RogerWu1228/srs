#pragma once
#include "tcp_base.h"
#include "../quick/quick.h"
#include "../common/stream_buffer.h"
#include <list>

// QuickPacket
struct QPacket {
	QPacket() {}
	QPacket(const QPacket& other) {
		session_id = other.session_id;
		data_len = other.data_len;
		data.reset(new uint8_t[data_len]);
		memcpy(data.get(), other.data.get(), data_len);
	}
	void Write(uint32_t session_id, const uint8_t* data, uint16_t data_len) {
		this->session_id = session_id;
		this->data_len = data_len;
		this->data.reset(new uint8_t[data_len]);
		memcpy(this->data.get(), data, data_len);
	}
	uint32_t session_id = 0;
	uint16_t data_len = 0;
	std::unique_ptr<uint8_t> data;
};
struct TcpPacket {
	TcpPacket() {}
	TcpPacket(const TcpPacket& other) {
		connected = false;
		disconnected = false;
		socket = other.socket;
		data_len = other.data_len;
		data.reset(new uint8_t[data_len]);
		memcpy(data.get(), other.data.get(), data_len);
	}
	void Write(TSOCKET socket, const uint8_t* data, uint16_t data_len) {
		connected = false;
		disconnected = false;
		this->socket = socket;
		this->data_len = data_len;
		this->data.reset(new uint8_t[data_len]);
		memcpy(this->data.get(), data, data_len);
	}

	bool connected = false;
	bool disconnected = false;
	TSOCKET socket = -1;
	uint16_t data_len = 0;
	std::unique_ptr<uint8_t> data;
};

class Connection
{
public:
	Connection(Quick* quick, TSOCKET socket, TSOCKET client_socket, uint32_t session_id, bool connected, bool quick_server_state);
	~Connection();
	TSOCKET socket() const { return _socket; }
	TSOCKET client_socket() const { return _client_socket; }
	uint32_t session_id() const { return _session_id; }
	bool DeliverTcpPackets();
	bool DeliverQuickPackets();
	void DeliverTcpPacket(const uint8_t *data, uint16_t data_len);
	void DeliverQuickPacket(const uint8_t* data, uint16_t data_len);
	void SetConnected(bool connected) { _connected = connected; }
	void SetQuickServerState(bool state) { _quick_server_state = state; }
	void TcpClose();
	void QuickClose();

	void SendTunnelConnectMsg();
	bool SendTunnelDisconnectMsg();

	bool CanSleep();
	void OutputConnectionStatus();

private:
	void TryDrainTcpPackets();
	void TryDrainQuickPackets();

	Quick* _quick = nullptr;
	TSOCKET _socket = -1;
	TSOCKET _client_socket = -1;
	uint32_t _session_id = 0;
	bool _connected = false;
	bool _quick_server_state = false;

	// 接收到的tcp数据，这些数据将会被通过quick发送出去。
	StreamBuffer _received_tcp_data;
	// 收到到的quick数据，这些数据将会通过tcp发送出去。
	StreamBuffer _received_quick_data;

	// 如果stream buffer满了，收到的包将被另外保存起来
	std::list<QPacket> _not_deliver_quick_packets;
	std::list<TcpPacket> _not_deliver_tcp_packets;

	uint64_t _total_tcp_received_bytes = 0;
	uint64_t _total_tcp_sent_bytes = 0;
	uint64_t _total_quick_received_bytes = 0;
	uint64_t _total_quick_sent_bytes = 0;

	uint8_t _buf[MAX_QUICK_PACKET_SIZE];
	uint8_t _fix_head_len = 0;
	bool _send_connect_msg = false;
	bool _need_send_tunnel_connect_msg = true;
};

