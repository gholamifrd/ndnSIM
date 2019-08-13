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

#include "pcon-strategy.hpp"

#include "str-helper.hpp"
#include "forwarder.hpp"
#include "../../common.hpp"
#include "../../../model/ndn-ns3.hpp"
#include "../../../ndn-cxx/src/util/face-uri.hpp"

namespace nfd {
namespace fw {

const Name PconStrategy::STRATEGY_NAME(
    "ndn:/localhost/nfd/strategy/pcon-strategy/%FD%01");
NFD_REGISTER_STRATEGY(PconStrategy);

// Used for logging:
shared_ptr<std::ofstream> PconStrategy::m_os;
boost::mutex PconStrategy::m_mutex;

PconStrategy::PconStrategy(Forwarder& forwarder, const Name& name)
    : Strategy(forwarder, name),
        m_ownForwarder(forwarder),
        m_lastFWRatioUpdate(time::steady_clock::TimePoint::min()),
        m_lastFWWrite(time::steady_clock::TimePoint::min()),
        sharedInfo(make_shared<MtForwardingInfo>()),
        TIME_BETWEEN_FW_UPDATE(time::milliseconds(110)),
        // 20ms between each writing of the forwarding table
        TIME_BETWEEN_FW_WRITE(time::milliseconds(20))
{
  // Write header of forwarding percentage table
  if (m_os == nullptr) {
    m_os = make_shared<std::ofstream>();
    m_os->open("results/fwperc.txt");
    *m_os << "Time" << "\tNode" << "\tPrefix" << "\tFaceId" << "\ttype" << "\tvalue"
        << "\n";
  }

  // Start all FIB entries by sending on the shortest path? 
  // If false: start with an equal split.
  INIT_SHORTEST_PATH = StrHelper::getEnvVariable("INIT_SHORTEST_PATH", true);

  // How much the forwarding percentage changes for each received congestion mark
  CHANGE_PER_MARK = StrHelper::getDoubleEnvVariable("CHANGE_PER_MARK", 0.02);

  // How much of the traffic should be used for probing?
  PROBING_PERCENTAGE = StrHelper::getDoubleEnvVariable("PROBING_PERCENTAGE", 0.001);
}

PconStrategy::~PconStrategy()
{
  m_os->close();
}

void
PconStrategy::afterReceiveInterest(const Face& inFace, const Interest& interest,
    shared_ptr<fib::Entry> fibEntry, shared_ptr<pit::Entry> pitEntry)
{

  // Retrieving measurement info
  shared_ptr<MtForwardingInfo> measurementInfo = StrHelper::getPrefixMeasurements(
      *fibEntry, this->getMeasurements());

  // Prefix info not found, create new prefix measurements
  if (measurementInfo == nullptr) {
    measurementInfo = StrHelper::addPrefixMeasurements(*fibEntry,
        this->getMeasurements());
    const Name prefixName = fibEntry->getPrefix();
    measurementInfo->setPrefix(prefixName.toUri());
    initializeForwMap(measurementInfo, fibEntry->getNextHops());
  }

  bool wantNewNonce = false;
  // Block similar interests that are not from the same host.

  // PIT entry for incoming Interest already exists.
  if (pitEntry->hasUnexpiredOutRecords()) {

    // Check if request comes from a new incoming face:
    bool requestFromNewFace = false;
    for (auto in : pitEntry->getInRecords()) {
      time::steady_clock::Duration lastRenewed = in.getLastRenewed()
          - time::steady_clock::now();
      if (lastRenewed.count() >= 0) {
        requestFromNewFace = true;
      }
    }

    // Received retx from same face. Forward with new nonce.
    if (!requestFromNewFace) {
      wantNewNonce = true;
    }
    // Request from new face. Suppress.
    else {
//      std::cout << StrHelper::getTime() << " Node " << m_ownForwarder.getNodeId()
//          << " suppressing request from new face: " << inFace.getId() << "!\n";
      return;
    }
  }

  shared_ptr<Face> outFace = nullptr;

  // Random number between 0 and 1.
  double r = ((double) rand() / (RAND_MAX));

  double percSum = 0;
  // Add all eligbile faces to list (excludes current downstream)
  std::vector<shared_ptr<Face>> eligbleFaces;
  for (auto n : fibEntry->getNextHops()) {
    if (StrHelper::predicate_NextHop_eligible(pitEntry, n, inFace.getId())) {
      eligbleFaces.push_back(n.getFace());
      // Add up percentage Sum.
      percSum += measurementInfo->getforwPerc(n.getFace()->getId());
    }
  }

  if (eligbleFaces.size() < 1) {
    std::cout << "Blocked interest from face: " << inFace.getId()
        << " (no eligible faces)\n";
    return;
  }

  // If only one face: Send out on it.
  else if (eligbleFaces.size() == 1) {
    outFace = eligbleFaces.front();
  }

  // More than 1 eligible face!
  else {
    // If percSum == 0, there is likely a problem in the routing configuration,
    // e.g. only the downstream has a forwPerc > 0.
    assert(percSum > 0);

    // Write fw percentage to file
    if (time::steady_clock::now() >= m_lastFWWrite + TIME_BETWEEN_FW_WRITE) {
      m_lastFWWrite = time::steady_clock::now();
      writeFwPercMap(m_ownForwarder, measurementInfo);
    }

    // Choose face according to current forwarding percentage:
    double forwPerc = 0;
    for (auto face : eligbleFaces) {
      forwPerc += measurementInfo->getforwPerc(face->getId()) / percSum;
      assert(forwPerc >= 0);
      assert(forwPerc <= 1.1);
      if (r < forwPerc) {
        outFace = face;
        break;
      }
    }
  }

  // Error: No possible outgoing face.
  if (outFace == nullptr) {
    std::cout << StrHelper::getTime() << " node " << m_ownForwarder.getNodeId()
        << ", inface " << inFace.getLocalUri() << inFace.getId() << ", interest: "
        << interest.getName() << " outface nullptr\n\n";
  }
  assert(outFace != nullptr);

  // If outgoing face is congested: mark PIT entry as congested. 
  if (StrHelper::isCongested(m_ownForwarder, outFace->getId())) {
    pitEntry->m_congMark = true;
    // TODO: Reduce forwarding percentage on outgoing face? 
    std::cout << StrHelper::getTime() << " node " << m_ownForwarder.getNodeId()
        << " marking PIT Entry " << pitEntry->getName() << ", inface: " << inFace.getId()
        << ", outface: " << outFace->getId() << "\n";
  }

  this->sendInterest(pitEntry, outFace, wantNewNonce);

  // Probe other faces
  if (r <= PROBING_PERCENTAGE) {
    probeInterests(outFace->getId(), fibEntry->getNextHops(), pitEntry);
  }
}

/**
 * Probe all faces other than the current outgoing face (which is used already).
 */
void
PconStrategy::probeInterests(const FaceId outFaceId, const fib::NextHopList& nexthops,
    shared_ptr<pit::Entry> pitEntry)
{
  for (auto n : nexthops) {
    if (n.getFace()->getId() != outFaceId) {
      this->sendInterest(pitEntry, n.getFace(), true);
    }
  }
}

void
PconStrategy::beforeSatisfyInterest(shared_ptr<pit::Entry> pitEntry, const Face& inFace,
    const Data& data)
{

  Name currentPrefix;
  shared_ptr<MtForwardingInfo> measurementInfo;
  std::tie(currentPrefix, measurementInfo) = StrHelper::findPrefixMeasurementsLPM(
      *pitEntry, this->getMeasurements());
  if (measurementInfo == nullptr) {
    std::cout << StrHelper::getTime() << "Didn't find measurement entry: "
        << pitEntry->getName() << "\n";
  }
  assert(measurementInfo != nullptr);

  int8_t congMark = StrHelper::getCongMark(data);
  int8_t nackType = StrHelper::getNackType(data);

  // If there is more than 1 face and data doesn't come from local content store
  // Then update forwarding ratio
  if (measurementInfo->getFaceCount() > 1 && !inFace.isLocal()
      && inFace.getLocalUri().toString() != "contentstore://") {

    bool updateBasedOnNACK = false;
    // For marked NACKs: Only adapt fw percentage each Update Interval (default: 110ms)
    if (nackType == NACK_TYPE_MARK
        && (time::steady_clock::now() >= m_lastFWRatioUpdate + TIME_BETWEEN_FW_UPDATE)) {
      m_lastFWRatioUpdate = time::steady_clock::now();
      updateBasedOnNACK = true;
    }

    // If Data congestion marked or NACK updates ratio:
    if (congMark || updateBasedOnNACK) {

      double fwPerc = measurementInfo->getforwPerc(inFace.getId());
      double change_perc = CHANGE_PER_MARK * fwPerc;

      StrHelper::reduceFwPerc(measurementInfo, inFace.getId(), change_perc);
      writeFwPercMap(m_ownForwarder, measurementInfo);
    }
  }

  bool pitMarkedCongested = false;
  if (pitEntry->m_congMark == true) {
    std::cout << StrHelper::getTime() << " node " << m_ownForwarder.getNodeId()
        << " found marked PIT Entry: " << pitEntry->getName() << ", face: "
        << inFace.getId() << "!\n";
    pitMarkedCongested = true;
  }

  for (auto n : pitEntry->getInRecords()) {
    bool downStreamCongested = StrHelper::isCongested(m_ownForwarder,
        n.getFace()->getId());

    int8_t markSentPacket = std::max(
        std::max((int8_t) congMark, (int8_t) downStreamCongested),
        (int8_t) pitMarkedCongested);

    ndn::CongestionTag tag3 = ndn::CongestionTag(nackType, markSentPacket, false, false);
    //    tag3.setCongMark(markSentPacket);
    assert(tag3.getCongMark() == markSentPacket);

    data.setTag(make_shared<ndn::CongestionTag>(tag3));
  }

}

/**
 * On expiring Interest: Reduce forwarding percentage of outgoing faces.
 * 
 * Note that in normal operations there shouldn't be many expired Interests.
 */
void
PconStrategy::beforeExpirePendingInterest(shared_ptr<pit::Entry> pitEntry)
{
  Name currentPrefix;
  shared_ptr<MtForwardingInfo> measurementInfo;
  std::tie(currentPrefix, measurementInfo) = StrHelper::findPrefixMeasurementsLPM(
      *pitEntry, this->getMeasurements());
  assert(measurementInfo != nullptr);

  // Reduce forwarding percentage of all expired OutRecords:
  for (auto o : pitEntry->getOutRecords()) {

    if (measurementInfo->getFaceCount() > 1
        && measurementInfo->getforwPerc(
            pitEntry->getOutRecords().front().getFace()->getId()) > 0.0) {

      std::cout << StrHelper::getTime() << " PIT Timeout : " << pitEntry->getName()
          << ", " << o.getFace()->getLocalUri() << o.getFace()->getId() << "! \n";

      StrHelper::reduceFwPerc(measurementInfo,
          pitEntry->getOutRecords().front().getFace()->getId(), CHANGE_PER_MARK);
      writeFwPercMap(m_ownForwarder, measurementInfo);
    }
  }
}

void
PconStrategy::initializeForwMap(shared_ptr<MtForwardingInfo> measurementInfo,
    std::vector<fib::NextHop> nexthops)
{
  int lowestId = std::numeric_limits<int>::max();

  int localFaceCount = 0;
  for (auto n : nexthops) {
    if (n.getFace()->isLocal()) {
      localFaceCount++;
    }
    if (n.getFace()->getId() < lowestId) {
      lowestId = n.getFace()->getId();
    }
  }

  std::cout << StrHelper::getTime() << " Init FW node " << m_ownForwarder.getNodeId()
      << ": ";
  for (auto n : nexthops) {
    double perc = 0.0;
    if (localFaceCount > 0 || INIT_SHORTEST_PATH) {
      if (n.getFace()->getId() == lowestId) {
        perc = 1.0;
      }
    }
    // Split forwarding percentage equally.
    else {
      perc = 1.0 / (double) nexthops.size();
    }
    std::cout << "face " << n.getFace()->getLocalUri() << n.getFace()->getId();
    std::cout << "=" << perc << ", ";
    measurementInfo->setforwPerc(n.getFace()->getId(), perc);
  }
  std::cout << "\n";

  writeFwPercMap(m_ownForwarder, measurementInfo);
}

void
PconStrategy::writeFwPercMap(Forwarder& ownForwarder,
    shared_ptr<MtForwardingInfo> measurementInfo)
{
  boost::mutex::scoped_lock scoped_lock(m_mutex);
  std::string prefix = measurementInfo->getPrefix();

  for (auto f : measurementInfo->getForwPercMap()) {
    StrHelper::printFwPerc(m_os, ownForwarder.getNodeId(), prefix, f.first, "forwperc",
        f.second);
  }
  m_os->flush();
}

} // namespace fw
} // namespace nfd
