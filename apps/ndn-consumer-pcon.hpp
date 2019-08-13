/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2016 Klaus Schneider, The University of Arizona
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Klaus Schneider <klaus@cs.arizona.edu>
 */

#ifndef NDN_CONSUMER_CC_H
#define NDN_CONSUMER_CC_H

#include "ndn-consumer.hpp"
#include "../../core/model/traced-value.h"
#include "../model/ndn-common.hpp"
#include "../utils/ndn-ns3-cc-tag.hpp"

namespace ns3 {
namespace ndn {
class Ns3CCTag;
} /* namespace ndn */
} /* namespace ns3 */

namespace ns3 {
class TypeId;
} /* namespace ns3 */

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-apps
 * \brief A consumer that reacts to congestion marks inside packets.
 *
 * Implements the Conservative Window Descrease mechanism and a TCP BIC window adaptation.
 *
 */
class ConsumerCC : public Consumer
{
public:
  static TypeId
  GetTypeId();

  ConsumerCC();

  // From App
  virtual void
  OnData(shared_ptr<const Data> contentObject);

  virtual void
  OnTimeout(uint32_t sequenceNumber);

  virtual void
  WillSendOutInterest(uint32_t sequenceNumber);

public:
  typedef void
  (*WindowTraceCallback)(uint32_t);

protected:

  virtual void
  ScheduleNextPacket();

private:
  virtual void
  SetWindow(uint32_t window);

  uint32_t
  GetWindow() const;

  double
  GetMaxSize() const;

  void
  SetMaxSize(double size);

  shared_ptr<::ns3::ndn::Ns3CCTag>
  getCCTag(shared_ptr<const Data> contentObject);

  bool
  getTagCongMarked(shared_ptr<const Data> contentObject);

  int
  getNackType(shared_ptr<const Data> contentObject);

  void
  windowDecrease(bool setInitialWindow = false);

  void
  bicIncrease();

  void
  bicDecrease(bool resetToInitial = false);

private:
  uint32_t m_payloadSize; // expected payload size
  double m_maxSize;       // max size to request
  uint32_t m_initialWindow; //

  TracedValue<double> m_cwnd;
  TracedValue<uint32_t> m_inFlight;

  double m_sstresh;

  // Variables for conservative window adaptation.
  uint64_t m_highData;
  uint64_t m_recoveryPoint;

  // Multiplicative Decrease factor
  double m_beta;

  /* BIC TCP Parameters */

  // Regular TCP behavior (including ss) until this window size
  const int LOW_WINDOW = 14;

  const int MAX_INCREMENT = 16;

  double bic_min_win; /* increase cwnd by 1 after ACKs */
  double bic_max_win; /* last maximum snd_cwnd */
  double bic_target_win; /* the last snd_cwnd */
  bool is_bic_ss;
  int bic_ss_cwnd;
  int bic_ss_target;

  RetxSeqsContainer m_timeoutSeqs;
  bool m_setInitialWindowOnTimeout; //
  bool m_reactToCongMarks;

  // Use conservative window adaptation (only one window decrease per RTT):
  bool m_consWindowAdapt;
  uint16_t m_maxMultiplier;

  bool m_init = false;

  unsigned m_minRtoInMS;
  unsigned m_rtoBonus;

  static const int NACK_TYPE_NONE = -1;
  static const int NACK_TYPE_LL_NO_MARK = 17;
  static const int NACK_TYPE_LL_MARK = 23;

};

} // namespace ndn
} // namespace ns3

#endif
