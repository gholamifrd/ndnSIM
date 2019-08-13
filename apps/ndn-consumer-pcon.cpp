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

#include "ndn-consumer-pcon.hpp"
#include "../ndn-cxx/src/util/time.hpp"
#include "../model/ndn-common.hpp"
#include "../utils/ndn-ns3-cc-tag.hpp"
#include "../utils/ndn-ns3-packet-tag.hpp"
#include "../../core/model/string.h"
#include "../../core/model/log.h"
#include "../../core/model/object-base.h"

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerCC");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerCC);

TypeId
ConsumerCC::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::ndn::ConsumerCC").SetGroupName("Ndn")
      .SetParent<Consumer>().AddConstructor<ConsumerCC>()

  .AddAttribute("Window", "Initial size of the window", StringValue("1"),
      MakeUintegerAccessor(&ConsumerCC::GetWindow, &ConsumerCC::SetWindow),
      MakeUintegerChecker<uint32_t>())

  .AddAttribute("PayloadSize",
      "Average size of content object size (to calculate interest generation rate)",
      UintegerValue(1040), MakeUintegerAccessor(&ConsumerCC::m_payloadSize),
      MakeUintegerChecker<uint32_t>())

  .AddAttribute("Size", "Amount of data in megabytes to request, relying on PayloadSize ",
      DoubleValue(-1),
      MakeDoubleAccessor(&ConsumerCC::GetMaxSize, &ConsumerCC::SetMaxSize),
      MakeDoubleChecker<double>())

  .AddAttribute("BETA", "Amount of data in megabytes to request, relying on PayloadSize ",
      DoubleValue(0.5), MakeDoubleAccessor(&ConsumerCC::m_beta),
      MakeDoubleChecker<double>())

  .AddAttribute("InitialWindowOnTimeout",
      "Set window to initial value when timeout occurs", BooleanValue(false),
      MakeBooleanAccessor(&ConsumerCC::m_setInitialWindowOnTimeout), MakeBooleanChecker())

  .AddAttribute("ReactToCongMarks",
      "If the Consumer App should react to congestion marks or not", BooleanValue(true),
      MakeBooleanAccessor(&ConsumerCC::m_reactToCongMarks), MakeBooleanChecker())

  .AddAttribute("UseConservativeWindowAdaptation",
      "If the Consumer App should use the conservative window adaptation or not",
      BooleanValue(true), MakeBooleanAccessor(&ConsumerCC::m_consWindowAdapt),
      MakeBooleanChecker())

  .AddAttribute("MaxMultiplier", "If the Consumer App should use the RTT Multiplier",
      UintegerValue(8), MakeUintegerAccessor(&ConsumerCC::m_maxMultiplier),
      MakeUintegerChecker<uint16_t>())

  .AddAttribute("MinRto", "Set the minimum RTO in milliseconds, ", UintegerValue(200),
      MakeUintegerAccessor(&ConsumerCC::m_minRtoInMS), MakeUintegerChecker<uint32_t>())

  .AddAttribute("MaxSeq",
      "Maximum sequence number to request (alternative to Size attribute, "
          "would activate only if Size is -1). "
          "The parameter is activated only if Size negative (not set)",
      IntegerValue(std::numeric_limits<uint32_t>::max()),
      MakeUintegerAccessor(&ConsumerCC::m_seqMax), MakeUintegerChecker<uint32_t>())

  .AddTraceSource("WindowTrace",
      "Window that controls how many outstanding interests are allowed",
      MakeTraceSourceAccessor(&ConsumerCC::m_cwnd),
      "ns3::ndn::ConsumerWindow::WindowTraceCallback").AddTraceSource("InFlight",
      "Current number of outstanding interests",
      MakeTraceSourceAccessor(&ConsumerCC::m_inFlight),
      "ns3::ndn::ConsumerWindow::WindowTraceCallback");

  return tid;
}

ConsumerCC::ConsumerCC()
    : m_payloadSize(1040),
        m_inFlight(0),
        m_sstresh(std::numeric_limits<int>::max()),
        m_highData(0),
        m_recoveryPoint(0),
        bic_min_win(0),
        bic_max_win(std::numeric_limits<int>::max()),
        bic_target_win(0),
        is_bic_ss(false),
        bic_ss_cwnd(0),
        bic_ss_target(0),
        m_minRtoInMS(1000)
{
}

void
ConsumerCC::SetWindow(uint32_t window)
{
  m_initialWindow = window;
  m_cwnd = m_initialWindow;
}

uint32_t
ConsumerCC::GetWindow() const
{
  return m_initialWindow;
}

double
ConsumerCC::GetMaxSize() const
{
  if (m_seqMax == 0)
    return -1.0;

  return m_maxSize;
}

void
ConsumerCC::SetMaxSize(double size)
{
  m_maxSize = size;
  if (m_maxSize < 0) {
    m_seqMax = 0;
    return;
  }

  m_seqMax = floor(1.0 + m_maxSize * 1024.0 * 1024.0 / m_payloadSize);
}

void
ConsumerCC::ScheduleNextPacket()
{
  if (m_cwnd <= static_cast<uint32_t>(0)) {
    std::cout << "Cwnd ran empty! Scheduling new packet!\n";
    Simulator::Remove(m_sendEvent);
    m_sendEvent = Simulator::Schedule(
        Seconds(std::min<double>(0.5, m_rtt->RetransmitTimeout().ToDouble(Time::S))),
        &Consumer::SendPacket, this);
  }
  else if (m_cwnd < m_inFlight) {
    // Cwnd smaller than inFlight: do nothing
  }
  // Cwnd larger than inFlight: send next packet
  else {
    if (m_sendEvent.IsRunning()) {
      Simulator::Remove(m_sendEvent);
    }
    m_sendEvent = Simulator::ScheduleNow(&Consumer::SendPacket, this);
  }
}

int
ConsumerCC::getNackType(shared_ptr<const Data> contentObject)
{
  auto ccTag = getCCTag(contentObject);
  if (ccTag != nullptr) {
    return ccTag->getNackType();
  }
  else {
    return -1;
  }
}

bool
ConsumerCC::getTagCongMarked(shared_ptr<const Data> contentObject)
{
  auto ccTag = getCCTag(contentObject);
  if (ccTag != nullptr) {
    return ccTag->getCongMark();
  }
  else {
    return false;
  }
}

shared_ptr<::ns3::ndn::Ns3CCTag>
ConsumerCC::getCCTag(shared_ptr<const Data> contentObject)
{
  auto tag = contentObject->getTag<Ns3PacketTag>();
  ns3::ndn::Ns3CCTag tempTag;
  if (tag != nullptr && tag->getPacket()->PeekPacketTag(tempTag)) {
    ns3::Ptr<const ns3::Packet> pkt = tag->getPacket();
    shared_ptr<ns3::ndn::Ns3CCTag> ccTag = make_shared<ns3::ndn::Ns3CCTag>();
    auto it = pkt->GetPacketTagIterator();
    int i = 0;
    while (it.HasNext()) {
      i++;
      auto n = it.Next();
      if (n.GetTypeId() == Ns3CCTag::GetTypeId()) {
        n.GetTag(*ccTag);
        break;
      }
    }
    return ccTag;
  }
  // Error.
  return nullptr;
}

void
ConsumerCC::windowDecrease(bool setInitialWindow)
{
  auto now = time::steady_clock::now().time_since_epoch();
  std::cout << time::duration_cast<time::milliseconds>(now) << " Node " << this->GetNode()->GetId()
      << " Cwnd decrease: " << m_cwnd << " -> " << m_cwnd * m_beta << "\n";
  bicDecrease(setInitialWindow);
}

void
ConsumerCC::OnData(shared_ptr<const Data> contentObject)
{
  if (!m_init) {
    m_init = true;
    m_rtt->setMaxMultiplier(m_maxMultiplier);
    m_rtt->SetMinRto(Time::FromInteger(m_minRtoInMS, Time::MS));

    std::cout << "Consumer PCON -- Min RTO: " << m_minRtoInMS << ", multiplier: "
        << m_maxMultiplier << "\n";
  }
  auto nameWithSequence = contentObject->getName();
  uint64_t sequenceNumber = nameWithSequence.get(-1).toSequenceNumber();

  // Set highest received data to seq. number
  if (m_highData < sequenceNumber) {
    m_highData = sequenceNumber;
  }

  const bool hasCongMark = getTagCongMarked(contentObject);

  int nackType = getNackType(contentObject);
  if (nackType > 0) {
    std::cout << " Consumer got NACK for seq: " << sequenceNumber << " -> Retx!\n";
    Consumer::OnNack(sequenceNumber);
  }
  else {
    Consumer::OnData(contentObject);
  }

  // If received marked Data or marked NACK: decrease window size
  if ((hasCongMark || nackType == NACK_TYPE_LL_MARK) && m_reactToCongMarks) {
    /* Don't use conservative window adaptation for congestion marks:
     * the AQM scheme (CoDel) is responsible for spreading out the marks.
     */
    if (hasCongMark || !m_consWindowAdapt) {
      windowDecrease();
    }
    else if (m_highData > m_recoveryPoint) {
      m_recoveryPoint = m_seq;
      windowDecrease();
    }
  }
  else {
    bicIncrease();
  }

  // Cwnd can never fall below initial window!
  m_cwnd = std::max(m_cwnd.Get(), (double) m_initialWindow);

  if (m_inFlight > static_cast<uint32_t>(0)) {
    m_inFlight--;
  }

  ScheduleNextPacket();
}

void
ConsumerCC::OnTimeout(uint32_t sequenceNumber)
{
  auto now = time::steady_clock::now().time_since_epoch();
  std::cout << time::duration_cast<time::milliseconds>(now) << " Timeout packet "
      << sequenceNumber << ", RTO: " << m_rtt->RetransmitTimeout().ToInteger(Time::MS)
      << " ms\n";

  // Reduce number of in flight packets.
  if (m_inFlight > static_cast<uint32_t>(0)) {
    m_inFlight--;
  }

  // Decide whether to decrease window size
  if (!m_consWindowAdapt) {
    windowDecrease(m_setInitialWindowOnTimeout);
  }
  else if (m_highData > m_recoveryPoint) {
    m_recoveryPoint = m_seq;
    windowDecrease(m_setInitialWindowOnTimeout);
  }

  Consumer::OnTimeout(sequenceNumber);
}

void
ConsumerCC::WillSendOutInterest(uint32_t sequenceNumber)
{
  m_inFlight++;
  Consumer::WillSendOutInterest(sequenceNumber);
}

void
ConsumerCC::bicIncrease()
{
  if (m_cwnd < LOW_WINDOW) {
    // Normal TCP
    if (m_cwnd <= m_sstresh) {
      m_cwnd = m_cwnd + 1;
    }
    else {
      m_cwnd = m_cwnd + (double) 1.0 / m_cwnd;
    }
  }
  else if (is_bic_ss == false) { // bin. increase
    //			std::cout << "BIC Increase, cwnd: " << m_cwnd << ", bic_target_win: " << bic_target_win << "\n";
    if (bic_target_win - m_cwnd < MAX_INCREMENT) { // binary search
      m_cwnd += (bic_target_win - m_cwnd) / m_cwnd;
    }
    else {
      m_cwnd += MAX_INCREMENT / m_cwnd; // additive increase
    }
    // FIX for equal double values.
    if (m_cwnd + 0.00001 < bic_max_win) {
      //				std::cout << "3 Cwnd: " << m_cwnd << ", bic_max_win: " << bic_max_win << "\n";
      bic_min_win = m_cwnd;
      //				std::cout << "bic_max_win: " << bic_max_win << ", bic_min_win: " << bic_min_win << "\n";
      bic_target_win = (bic_max_win + bic_min_win) / 2;
    }
    else {
      is_bic_ss = true;
      bic_ss_cwnd = 1;
      bic_ss_target = m_cwnd + 1;
      bic_max_win = std::numeric_limits<int>::max();
    }
  }
  // BICslow start
  else {
    m_cwnd += bic_ss_cwnd / m_cwnd;
    if (m_cwnd >= bic_ss_target) {
      bic_ss_cwnd = 2 * bic_ss_cwnd;
      bic_ss_target = m_cwnd + bic_ss_cwnd;
    }
    if (bic_ss_cwnd >= MAX_INCREMENT) {
      is_bic_ss = false;
    }
  }

}

void
ConsumerCC::bicDecrease(bool resetToInitial)
{
  // BIC Decrease
  if (m_cwnd >= LOW_WINDOW) {
    auto prev_max = bic_max_win;
    bic_max_win = m_cwnd;
    m_cwnd = m_cwnd * m_beta;
    bic_min_win = m_cwnd;
    if (prev_max > bic_max_win) //Fast. Conv.
      bic_max_win = (bic_max_win + bic_min_win) / 2;
    bic_target_win = (bic_max_win + bic_min_win) / 2;
  }
  else {
    // Normal TCP
    m_sstresh = m_cwnd * m_beta;
    m_cwnd = m_sstresh;
  }

  if (resetToInitial) {
    m_sstresh = m_cwnd * m_beta;
    m_cwnd = m_initialWindow;
  }
}

} // namespace ndn
} // namespace ns3
