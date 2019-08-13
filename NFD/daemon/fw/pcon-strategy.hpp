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

#ifndef NFD_DAEMON_FW_PCON_STRATEGY_HPP
#define NFD_DAEMON_FW_PCON_STRATEGY_HPP

#include "strategy.hpp"
#include "mt-forwarding-info.hpp"
#include <boost/thread/pthread/mutex.hpp>
#include <fstream>

namespace nfd {
namespace fw {

/** \brief Congestion Control Strategy
 * 
 */
class PconStrategy: public Strategy
{
public:

  PconStrategy(Forwarder& forwarder, const Name& name = STRATEGY_NAME);

  virtual
  ~PconStrategy();

  virtual void
  afterReceiveInterest(const Face& inFace, const Interest& interest,
      shared_ptr<fib::Entry> fibEntry, shared_ptr<pit::Entry> pitEntry) DECL_OVERRIDE;

  virtual void
  beforeSatisfyInterest(shared_ptr<pit::Entry> pitEntry, const Face& inFace,
      const Data& data) DECL_OVERRIDE;

  virtual void
  beforeExpirePendingInterest(shared_ptr<pit::Entry> pitEntry) DECL_OVERRIDE;

  /**
   * Sends out probed interest packets with a new nonce.
   * Currently all paths except the working path are probed whenever probingDue() returns true.
   */
  void
  probeInterests(const FaceId outFaceId, const fib::NextHopList& nexthops,
      shared_ptr<pit::Entry> pitEntry);

public:
  static const Name STRATEGY_NAME;

private:

  void
  printQueue();
  int
  getQueueOfId(Forwarder &forwarder, int faceid);

  static void
  writeFwPercMap(Forwarder& ownForwarder, shared_ptr<MtForwardingInfo> measurementInfo);

  void
  initializeForwMap(shared_ptr<MtForwardingInfo> measurementInfo,
      std::vector<fib::NextHop> nexthops);

public:

  static const int8_t NACK_TYPE_NONE = -1;
  static const int8_t NACK_TYPE_NO_MARK = 17;
  static const int8_t NACK_TYPE_MARK = 23;

private:

  static shared_ptr<std::ofstream> m_os;
  static boost::mutex m_mutex;

  Forwarder& m_ownForwarder;
  time::steady_clock::TimePoint m_lastFWRatioUpdate;
  time::steady_clock::TimePoint m_lastFWWrite;

  bool INIT_SHORTEST_PATH;
  double PROBING_PERCENTAGE;
  double CHANGE_PER_MARK;

//  bool USE_HIGH_CONG_THRESH;

  shared_ptr<MtForwardingInfo> sharedInfo;

  const time::steady_clock::duration TIME_BETWEEN_FW_UPDATE;
  const time::steady_clock::duration TIME_BETWEEN_FW_WRITE;
};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_PCON_STRATEGY_HPP
