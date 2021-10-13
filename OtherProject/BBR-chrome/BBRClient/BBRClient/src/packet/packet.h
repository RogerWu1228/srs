#ifndef PACKET_H
#define PACKET_H 1

#include <stddef.h>
#include <vector>
#include <string>
#include "defs.h"
#include "quic_types.h"
#include "quic_time.h"

enum PACKET_TYPE{
    DATA,
    INFO,
};

class Packet
{
public:
    Packet(/* args */);
    // ~Packet();
    virtual const std::string   Serialize()=0;
};

// send time
class SerializedPacket:public Packet
{
public:
    SerializedPacket(/* args */);
    // ~SerializedPacket();

    const std::string  Serialize() override;
    
    QuicPacketNumber packet_number;
    QuicPacketLength encrypted_length=DEFAULT_PACKET_SIZE;
    QuicTime send_time;
};



#endif
