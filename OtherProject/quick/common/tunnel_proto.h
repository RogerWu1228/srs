#pragma once
#include <memory>
#pragma pack(push, 1)
enum {
	TUNNEL_MSG_CONNECT = 0,
	TUNNEL_MSG_DISCONNECT,
	TUNNEL_MSG_PAYLOAD,
};

struct TunnelConnect {
	uint8_t msg_id = TUNNEL_MSG_CONNECT;
	uint32_t socket = 0;
};

struct TunnelDisconnect {
	uint8_t msg_id = TUNNEL_MSG_DISCONNECT;
	uint32_t socket = 0;
};

struct TunnelPayload {
	uint8_t msg_id = TUNNEL_MSG_PAYLOAD;
	uint32_t socket = 0;
	uint8_t payload[1];
};

#pragma pack(pop)
