// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_QUIC_TYPES_H_
#define QUICHE_QUIC_CORE_QUIC_TYPES_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <ostream>
#include <vector>
#include <memory>

#include "quic_packet_number.h"
#include "quic_time.h"

//####################### this origin locate in quic_container.h
template <typename T, size_t N, typename A = std::allocator<T>>
using QuicInlinedVector = std::vector<T, A>;
//###################### 

using QuicRoundTripCount = uint64_t;

using QuicPacketLength = uint16_t;
using QuicControlFrameId = uint32_t;
using QuicMessageId = uint32_t;
using QuicDatagramFlowId = uint64_t;

using QuicByteCount = uint64_t;
using QuicPacketCount = uint64_t;
using QuicPublicResetNonceProof = uint64_t;
using QuicStreamOffset = uint64_t;
using DiversificationNonce = std::array<char, 32>;
using PacketTimeVector = std::vector<std::pair<QuicPacketNumber, QuicTime>>;


enum TransmissionType : int8_t {
  NOT_RETRANSMISSION,
  FIRST_TRANSMISSION_TYPE = NOT_RETRANSMISSION,
  HANDSHAKE_RETRANSMISSION,     // Retransmits due to handshake timeouts.
  ALL_ZERO_RTT_RETRANSMISSION,  // Retransmits all packets encrypted with 0-RTT
                                // key.
  LOSS_RETRANSMISSION,          // Retransmits due to loss detection.
  RTO_RETRANSMISSION,           // Retransmits due to retransmit time out.
  TLP_RETRANSMISSION,           // Tail loss probes.
  PTO_RETRANSMISSION,           // Retransmission due to probe timeout.
  PROBING_RETRANSMISSION,       // Retransmission in order to probe bandwidth.
  PATH_RETRANSMISSION,          // Retransmission proactively due to underlying
                                // network change.
  ALL_INITIAL_RETRANSMISSION,   // Retransmit all packets encrypted with INITIAL
                                // key.
  LAST_TRANSMISSION_TYPE = ALL_INITIAL_RETRANSMISSION,
};

 std::string TransmissionTypeToString(
    TransmissionType transmission_type);

 std::ostream& operator<<(
    std::ostream& os,
    TransmissionType transmission_type);

enum HasRetransmittableData : uint8_t {
  NO_RETRANSMITTABLE_DATA,
  HAS_RETRANSMITTABLE_DATA,
};

enum class Perspective : uint8_t { IS_SERVER, IS_CLIENT };

 std::string PerspectiveToString(Perspective perspective);
 std::ostream& operator<<(std::ostream& os,
                                             const Perspective& perspective);
enum QuicFrameType : uint8_t {
  // Regular frame types. The values set here cannot change without the
  // introduction of a new QUIC version.
  PADDING_FRAME = 0,
  RST_STREAM_FRAME = 1,
  CONNECTION_CLOSE_FRAME = 2,
  GOAWAY_FRAME = 3,
  WINDOW_UPDATE_FRAME = 4,
  BLOCKED_FRAME = 5,
  STOP_WAITING_FRAME = 6,
  PING_FRAME = 7,
  CRYPTO_FRAME = 8,
  // TODO(b/157935330): stop hard coding this when deprecate T050.
  HANDSHAKE_DONE_FRAME = 9,

  // STREAM and ACK frames are special frames. They are encoded differently on
  // the wire and their values do not need to be stable.
  STREAM_FRAME,
  ACK_FRAME,
  // The path MTU discovery frame is encoded as a PING frame on the wire.
  MTU_DISCOVERY_FRAME,

  // These are for IETF-specific frames for which there is no mapping
  // from Google QUIC frames. These are valid/allowed if and only if IETF-
  // QUIC has been negotiated. Values are not important, they are not
  // the values that are in the packets (see QuicIetfFrameType, below).
  NEW_CONNECTION_ID_FRAME,
  MAX_STREAMS_FRAME,
  STREAMS_BLOCKED_FRAME,
  PATH_RESPONSE_FRAME,
  PATH_CHALLENGE_FRAME,
  STOP_SENDING_FRAME,
  MESSAGE_FRAME,
  NEW_TOKEN_FRAME,
  RETIRE_CONNECTION_ID_FRAME,
  ACK_FREQUENCY_FRAME,

  NUM_FRAME_TYPES
};

// Human-readable string suitable for logging.
 std::string QuicFrameTypeToString(QuicFrameType t);
 std::ostream& operator<<(std::ostream& os,
                                             const QuicFrameType& t);

// Ietf frame types. These are defined in the IETF QUIC Specification.
// Explicit values are given in the enum so that we can be sure that
// the symbol will map to the correct stream type.
// All types are defined here, even if we have not yet implmented the
// quic/core/stream/.... stuff needed.
// Note: The protocol specifies that frame types are varint-62 encoded,
// further stating that the shortest encoding must be used.  The current set of
// frame types all have values less than 0x40 (64) so can be encoded in a single
// byte, with the two most significant bits being 0. Thus, the following
// enumerations are valid as both the numeric values of frame types AND their
// encodings.
enum QuicIetfFrameType : uint64_t {
  IETF_PADDING = 0x00,
  IETF_PING = 0x01,
  IETF_ACK = 0x02,
  IETF_ACK_ECN = 0x03,
  IETF_RST_STREAM = 0x04,
  IETF_STOP_SENDING = 0x05,
  IETF_CRYPTO = 0x06,
  IETF_NEW_TOKEN = 0x07,
  // the low-3 bits of the stream frame type value are actually flags
  // declaring what parts of the frame are/are-not present, as well as
  // some other control information. The code would then do something
  // along the lines of "if ((frame_type & 0xf8) == 0x08)" to determine
  // whether the frame is a stream frame or not, and then examine each
  // bit specifically when/as needed.
  IETF_STREAM = 0x08,
  // 0x09 through 0x0f are various flag settings of the IETF_STREAM frame.
  IETF_MAX_DATA = 0x10,
  IETF_MAX_STREAM_DATA = 0x11,
  IETF_MAX_STREAMS_BIDIRECTIONAL = 0x12,
  IETF_MAX_STREAMS_UNIDIRECTIONAL = 0x13,
  IETF_DATA_BLOCKED = 0x14,
  IETF_STREAM_DATA_BLOCKED = 0x15,
  IETF_STREAMS_BLOCKED_BIDIRECTIONAL = 0x16,
  IETF_STREAMS_BLOCKED_UNIDIRECTIONAL = 0x17,
  IETF_NEW_CONNECTION_ID = 0x18,
  IETF_RETIRE_CONNECTION_ID = 0x19,
  IETF_PATH_CHALLENGE = 0x1a,
  IETF_PATH_RESPONSE = 0x1b,
  // Both of the following are "Connection Close" frames,
  // the first signals transport-layer errors, the second application-layer
  // errors.
  IETF_CONNECTION_CLOSE = 0x1c,
  IETF_APPLICATION_CLOSE = 0x1d,

  IETF_HANDSHAKE_DONE = 0x1e,

  // The MESSAGE frame type has not yet been fully standardized.
  // QUIC versions starting with 46 and before 99 use 0x20-0x21.
  // IETF QUIC (v99) uses 0x30-0x31, see draft-pauly-quic-datagram.
  IETF_EXTENSION_MESSAGE_NO_LENGTH = 0x20,
  IETF_EXTENSION_MESSAGE = 0x21,
  IETF_EXTENSION_MESSAGE_NO_LENGTH_V99 = 0x30,
  IETF_EXTENSION_MESSAGE_V99 = 0x31,

  // An QUIC extension frame for sender control of acknowledgement delays
  IETF_ACK_FREQUENCY = 0xaf
};
 std::ostream& operator<<(std::ostream& os,
                                             const QuicIetfFrameType& c);
 std::string QuicIetfFrameTypeString(QuicIetfFrameType t);

// Masks for the bits that indicate the frame is a Stream frame vs the
// bits used as flags.
#define IETF_STREAM_FRAME_TYPE_MASK 0xfffffffffffffff8
#define IETF_STREAM_FRAME_FLAG_MASK 0x07
#define IS_IETF_STREAM_FRAME(_stype_) \
  (((_stype_)&IETF_STREAM_FRAME_TYPE_MASK) == IETF_STREAM)

// These are the values encoded in the low-order 3 bits of the
// IETF_STREAMx frame type.
#define IETF_STREAM_FRAME_FIN_BIT 0x01
#define IETF_STREAM_FRAME_LEN_BIT 0x02
#define IETF_STREAM_FRAME_OFF_BIT 0x04


// Defines for all types of congestion control algorithms that can be used in
// QUIC. Note that this is separate from the congestion feedback type -
// some congestion control algorithms may use the same feedback type
// (Reno and Cubic are the classic example for that).
enum CongestionControlType {
  kCubicBytes,
  kRenoBytes,
  kBBR,
  kPCC,
  kGoogCC,
  kBBRv2,
};

// EncryptionLevel enumerates the stages of encryption that a QUIC connection
// progresses through. When retransmitting a packet, the encryption level needs
// to be specified so that it is retransmitted at a level which the peer can
// understand.
enum EncryptionLevel : int8_t {
  ENCRYPTION_INITIAL = 0,
  ENCRYPTION_HANDSHAKE = 1,
  ENCRYPTION_ZERO_RTT = 2,
  ENCRYPTION_FORWARD_SECURE = 3,

  NUM_ENCRYPTION_LEVELS,
};

enum SentPacketState : uint8_t {
  // The packet is in flight and waiting to be acked.
  OUTSTANDING,
  FIRST_PACKET_STATE = OUTSTANDING,
  // The packet was never sent.
  NEVER_SENT,
  // The packet has been acked.
  ACKED,
  // This packet is not expected to be acked.
  UNACKABLE,
  // This packet has been delivered or unneeded.
  NEUTERED,

  // States below are corresponding to retransmission types in TransmissionType.

  // This packet has been retransmitted when retransmission timer fires in
  // HANDSHAKE mode.
  HANDSHAKE_RETRANSMITTED,
  // This packet is considered as lost, this is used for LOST_RETRANSMISSION.
  LOST,
  // This packet has been retransmitted when TLP fires.
  TLP_RETRANSMITTED,
  // This packet has been retransmitted when RTO fires.
  RTO_RETRANSMITTED,
  // This packet has been retransmitted when PTO fires.
  PTO_RETRANSMITTED,
  // This packet has been retransmitted for probing purpose.
  PROBE_RETRANSMITTED,
  // This packet is sent on a different path or is a PING only packet.
  // Do not update RTT stats and congestion control if the packet is the
  // largest_acked of an incoming ACK.
  NOT_CONTRIBUTING_RTT,
  LAST_PACKET_STATE = NOT_CONTRIBUTING_RTT,
};


// Information about a newly acknowledged packet.
struct  AckedPacket {
   AckedPacket(QuicPacketNumber packet_number,
                        QuicPacketLength bytes_acked,
                        QuicTime receive_timestamp)
      : packet_number(packet_number),
        bytes_acked(bytes_acked),
        receive_timestamp(receive_timestamp) {}

  friend  std::ostream& operator<<(
      std::ostream& os,
      const AckedPacket& acked_packet);

  QuicPacketNumber packet_number;
  // Number of bytes sent in the packet that was acknowledged.
  QuicPacketLength bytes_acked;
  // The time |packet_number| was received by the peer, according to the
  // optional timestamp the peer included in the ACK frame which acknowledged
  // |packet_number|. Zero if no timestamp was available for this packet.
  QuicTime receive_timestamp;
};

// A vector of acked packets.
using AckedPacketVector = QuicInlinedVector<AckedPacket, 2>;

// Information about a newly lost packet.
struct  LostPacket {
  LostPacket(QuicPacketNumber packet_number, QuicPacketLength bytes_lost)
      : packet_number(packet_number), bytes_lost(bytes_lost) {}

  friend  std::ostream& operator<<(
      std::ostream& os,
      const LostPacket& lost_packet);

  QuicPacketNumber packet_number;
  // Number of bytes sent in the packet that was lost.
  QuicPacketLength bytes_lost;
};

// A vector of lost packets.
using LostPacketVector = QuicInlinedVector<LostPacket, 2>;

// A packet number space is the context in which a packet can be processed and
// acknowledged.
enum PacketNumberSpace : uint8_t {
  INITIAL_DATA = 0,  // Only used in IETF QUIC.
  HANDSHAKE_DATA = 1,
  APPLICATION_DATA = 2,

  NUM_PACKET_NUMBER_SPACES,
};

 std::string PacketNumberSpaceToString(
    PacketNumberSpace packet_number_space);

// Used to return the result of processing a received ACK frame.
enum AckResult {
  PACKETS_NEWLY_ACKED,
  NO_PACKETS_NEWLY_ACKED,
  UNSENT_PACKETS_ACKED,     // Peer acks unsent packets.
  UNACKABLE_PACKETS_ACKED,  // Peer acks packets that are not expected to be
                            // acked. For example, encryption is reestablished,
                            // and all sent encrypted packets cannot be
                            // decrypted by the peer. Version gets negotiated,
                            // and all sent packets in the different version
                            // cannot be processed by the peer.
  PACKETS_ACKED_IN_WRONG_PACKET_NUMBER_SPACE,
};





#endif  // QUICHE_QUIC_CORE_QUIC_TYPES_H_
