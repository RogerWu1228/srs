/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "src/google_cc/delay_based_bwe.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "src/time/time_delta.h"
#include "src/google_cc/bwe_defines.h"
#include "src/google_cc/trendline_estimator.h"

namespace webrtc {
namespace {
constexpr TimeDelta kStreamTimeOut = TimeDelta::Seconds(2);

// Used with field trial "WebRTC-Bwe-NewInterArrivalDelta/Enabled/
constexpr TimeDelta kSendTimeGroupLength = TimeDelta::Millis(5);

// Used unless field trial "WebRTC-Bwe-NewInterArrivalDelta/Enabled/"
constexpr int kTimestampGroupLengthMs = 5;
constexpr int kAbsSendTimeFraction = 18;
constexpr int kAbsSendTimeInterArrivalUpshift = 8;
constexpr int kInterArrivalShift =
    kAbsSendTimeFraction + kAbsSendTimeInterArrivalUpshift;
constexpr int kTimestampGroupTicks =
    (kTimestampGroupLengthMs << kInterArrivalShift) / 1000;
constexpr double kTimestampToMs =
    1000.0 / static_cast<double>(1 << kInterArrivalShift);

// This ssrc is used to fulfill the current API but will be removed
// after the API has been changed.
constexpr uint32_t kFixedSsrc = 0;
}  // namespace

constexpr char BweSeparateAudioPacketsSettings::kKey[];

DelayBasedBwe::Result::Result()
    : updated(false),
      probe(false),
      target_bitrate(DataRate::Zero()),
      recovered_from_overuse(false),
      backoff_in_alr(false) {}

DelayBasedBwe::DelayBasedBwe()
    : 
      separate_audio_(),
      audio_packets_since_last_video_(0),
      last_video_packet_recv_time_(Timestamp::MinusInfinity()),
      video_delay_detector_(
          new TrendlineEstimator()),
      audio_delay_detector_(
          new TrendlineEstimator()),
      active_delay_detector_(video_delay_detector_.get()),
      last_seen_packet_(Timestamp::MinusInfinity()),
      rate_control_(/*send_side=*/true),
      prev_bitrate_(DataRate::Zero()),
      has_once_detected_overuse_(false),
      prev_state_(BandwidthUsage::kBwNormal),
      use_new_inter_arrival_delta_(true){
}

DelayBasedBwe::~DelayBasedBwe() {}

DelayBasedBwe::Result DelayBasedBwe::IncomingPacketFeedbackVector(
    const TransportPacketsFeedback& msg,
    absl::optional<DataRate> acked_bitrate,
    absl::optional<NetworkStateEstimate> network_estimate,
    bool in_alr) {
  auto packet_feedback_vector = msg.SortedByReceiveTime();
  // TODO(holmer): An empty feedback vector here likely means that
  // all acks were too late and that the send time history had
  // timed out. We should reduce the rate when this occurs.
  if (packet_feedback_vector.empty()) {
    return DelayBasedBwe::Result();
  }

  bool delayed_feedback = true;
  bool recovered_from_overuse = false;
  BandwidthUsage prev_detector_state = active_delay_detector_->State();
  for (const auto& packet_feedback : packet_feedback_vector) {
    delayed_feedback = false;
    IncomingPacketFeedback(packet_feedback, msg.feedback_time);
    if (prev_detector_state == BandwidthUsage::kBwUnderusing &&
        active_delay_detector_->State() == BandwidthUsage::kBwNormal) {
      recovered_from_overuse = true;
    }
    prev_detector_state = active_delay_detector_->State();
  }

  if (delayed_feedback) {
    // TODO(bugs.webrtc.org/10125): Design a better mechanism to safe-guard
    // against building very large network queues.
    return Result();
  }
  rate_control_.SetInApplicationLimitedRegion(in_alr);
  rate_control_.SetNetworkStateEstimate(network_estimate);
  return MaybeUpdateEstimate(acked_bitrate, 
                             std::move(network_estimate),
                             recovered_from_overuse, in_alr, msg.feedback_time);
}

void DelayBasedBwe::IncomingPacketFeedback(const PacketResult& packet_feedback,
                                           Timestamp at_time) {
  // Reset if the stream has timed out.
  if (last_seen_packet_.IsInfinite() ||
      at_time - last_seen_packet_ > kStreamTimeOut) {
    if (use_new_inter_arrival_delta_) {
      video_inter_arrival_delta_ =
          std::make_unique<InterArrivalDelta>(kSendTimeGroupLength);
      audio_inter_arrival_delta_ =
          std::make_unique<InterArrivalDelta>(kSendTimeGroupLength);
    } 
    video_delay_detector_.reset(
        new TrendlineEstimator());
    audio_delay_detector_.reset(
        new TrendlineEstimator());
    active_delay_detector_ = video_delay_detector_.get();
  }
  last_seen_packet_ = at_time;

  // As an alternative to ignoring small packets, we can separate audio and
  // video packets for overuse detection.
  DelayIncreaseDetectorInterface* delay_detector_for_packet =
      video_delay_detector_.get();
  if (separate_audio_.enabled) {
    if (packet_feedback.sent_packet.audio) {
      delay_detector_for_packet = audio_delay_detector_.get();
      audio_packets_since_last_video_++;
      if (audio_packets_since_last_video_ > separate_audio_.packet_threshold &&
          packet_feedback.receive_time - last_video_packet_recv_time_ >
              separate_audio_.time_threshold) {
        active_delay_detector_ = audio_delay_detector_.get();
      }
    } else {
      audio_packets_since_last_video_ = 0;
      last_video_packet_recv_time_ =
          std::max(last_video_packet_recv_time_, packet_feedback.receive_time);
      active_delay_detector_ = video_delay_detector_.get();
    }
  }
  DataSize packet_size = packet_feedback.sent_packet.size;

  if (use_new_inter_arrival_delta_) {
    TimeDelta send_delta = TimeDelta::Zero();
    TimeDelta recv_delta = TimeDelta::Zero();
    int size_delta = 0;

    InterArrivalDelta* inter_arrival_for_packet =
        (separate_audio_.enabled && packet_feedback.sent_packet.audio)
            ? video_inter_arrival_delta_.get()
            : audio_inter_arrival_delta_.get();
    bool calculated_deltas = inter_arrival_for_packet->ComputeDeltas(
        packet_feedback.sent_packet.send_time, packet_feedback.receive_time,
        at_time, packet_size.bytes(), &send_delta, &recv_delta, &size_delta);

    delay_detector_for_packet->Update(
        recv_delta.ms(), send_delta.ms(),
        packet_feedback.sent_packet.send_time.ms(),
        packet_feedback.receive_time.ms(), packet_size.bytes(),
        calculated_deltas);
  }
}

DataRate DelayBasedBwe::TriggerOveruse(Timestamp at_time,
                                       absl::optional<DataRate> link_capacity) {
  RateControlInput input(BandwidthUsage::kBwOverusing, link_capacity);
  return rate_control_.Update(&input, at_time);
}

DelayBasedBwe::Result DelayBasedBwe::MaybeUpdateEstimate(
    absl::optional<DataRate> acked_bitrate,
    absl::optional<NetworkStateEstimate> state_estimate,
    bool recovered_from_overuse,
    bool in_alr,
    Timestamp at_time) {
  Result result;

  // Currently overusing the bandwidth.
  if (active_delay_detector_->State() == BandwidthUsage::kBwOverusing) {
    if (has_once_detected_overuse_ && in_alr /*&& alr_limited_backoff_enabled_*/) {
      if (rate_control_.TimeToReduceFurther(at_time, prev_bitrate_)) {
        result.updated =
            UpdateEstimate(at_time, prev_bitrate_, &result.target_bitrate);
        result.backoff_in_alr = true;
      }
    } else if (acked_bitrate &&
               rate_control_.TimeToReduceFurther(at_time, *acked_bitrate)) {
      result.updated =
          UpdateEstimate(at_time, acked_bitrate, &result.target_bitrate);
    } else if (!acked_bitrate && rate_control_.ValidEstimate() &&
               rate_control_.InitialTimeToReduceFurther(at_time)) {
      // Overusing before we have a measured acknowledged bitrate. Reduce send
      // rate by 50% every 200 ms.
      // TODO(tschumim): Improve this and/or the acknowledged bitrate estimator
      // so that we (almost) always have a bitrate estimate.
      rate_control_.SetEstimate(rate_control_.LatestEstimate() / 2, at_time);
      result.updated = true;
      result.probe = false;
      result.target_bitrate = rate_control_.LatestEstimate();
    }
    has_once_detected_overuse_ = true;
  } else {
    if (false /*probe_bitrate*/) {
    } else {
      result.updated =
          UpdateEstimate(at_time, acked_bitrate, &result.target_bitrate);
      result.recovered_from_overuse = recovered_from_overuse;
    }
  }
  BandwidthUsage detector_state = active_delay_detector_->State();
  if ((result.updated && prev_bitrate_ != result.target_bitrate) ||
      detector_state != prev_state_) {
    DataRate bitrate = result.updated ? result.target_bitrate : prev_bitrate_;

    prev_bitrate_ = bitrate;
    prev_state_ = detector_state;
  }
  return result;
}

bool DelayBasedBwe::UpdateEstimate(Timestamp at_time,
                                   absl::optional<DataRate> acked_bitrate,
                                   DataRate* target_rate) {
  const RateControlInput input(active_delay_detector_->State(), acked_bitrate);
  *target_rate = rate_control_.Update(&input, at_time);
  return rate_control_.ValidEstimate();
}

void DelayBasedBwe::OnRttUpdate(TimeDelta avg_rtt) {
  rate_control_.SetRtt(avg_rtt);
}

bool DelayBasedBwe::LatestEstimate(std::vector<uint32_t>* ssrcs,
                                   DataRate* bitrate) const {
  // Currently accessed from both the process thread (see
  // ModuleRtpRtcpImpl::Process()) and the configuration thread (see
  // Call::GetStats()). Should in the future only be accessed from a single
  // thread.
  if (!rate_control_.ValidEstimate())
    return false;

  *ssrcs = {kFixedSsrc};
  *bitrate = rate_control_.LatestEstimate();
  return true;
}

void DelayBasedBwe::SetStartBitrate(DataRate start_bitrate) {
  rate_control_.SetStartBitrate(start_bitrate);
}

void DelayBasedBwe::SetMinBitrate(DataRate min_bitrate) {
  // Called from both the configuration thread and the network thread. Shouldn't
  // be called from the network thread in the future.
  rate_control_.SetMinBitrate(min_bitrate);
}

TimeDelta DelayBasedBwe::GetExpectedBwePeriod() const {
  return rate_control_.GetExpectedBandwidthPeriod();
}

void DelayBasedBwe::SetAlrLimitedBackoffExperiment(bool enabled) {
}

}  // namespace webrtc
