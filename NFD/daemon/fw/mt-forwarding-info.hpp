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

#ifndef NFD_DAEMON_FW_MT_FORWARDING_INFO_HPP
#define NFD_DAEMON_FW_MT_FORWARDING_INFO_HPP

#include "../face/face.hpp"
#include "strategy-info.hpp"

namespace nfd {
namespace fw {

/**
 * Measurement information that can be saved and retrieved per-name-prefix.
 */
class MtForwardingInfo : public StrategyInfo
{
public:

  static constexpr int
  getTypeId()
  {
    return 1013;
  }

  MtForwardingInfo()
      : m_ownPrefix("null")
  {
  }

  double
  getforwPerc(FaceId face)
  {
    if (m_forwPercMap.find(face) == m_forwPercMap.end()) {
      std::cout << time::steady_clock::now().time_since_epoch().count() / 1000 * 1000
          << " ms, couldn't find face " << face << "\n";
    }
    assert(m_forwPercMap.find(face) != m_forwPercMap.end());
    double forwPerc = m_forwPercMap.at(face);
    assert(forwPerc >= 0 && forwPerc <= 1);

    return forwPerc;
  }

  void
  setforwPerc(FaceId faceId, double perc)
  {
    m_forwPercMap[faceId] = perc;
  }

  void
  increaseforwPerc(FaceId faceId, double changeRate)
  {
    m_forwPercMap[faceId] += changeRate;
  }

  const std::map<FaceId, double>
  getForwPercMap() const
  {
    return m_forwPercMap;
  }

  int
  getFaceCount()
  {
    std::vector<FaceId> faceIdList;
    for (auto faceInfo : m_forwPercMap) {
      faceIdList.push_back(faceInfo.first);
    }
    return faceIdList.size();
  }

  void
  setPrefix(std::string prefix)
  {
    m_ownPrefix = prefix;
  }

  std::string
  getPrefix() const
  {
    return m_ownPrefix;
  }

private:

  /** These Functions were used to disable faces in the "highly congested" state:*/
  //  void
  //  enableFace(FaceId faceId)
  //  {
  //    m_disabledFaces.erase(faceId);
  //  }
  //
  //  void
  //  disableFace(FaceId faceId)
  //  {
  //    m_disabledFaces.emplace(faceId);
  ////      std::cout << "Disabling face " << faceId << "\n";
  //  }
  //
  //  bool
  //  isFaceEnabled(FaceId faceId)
  //  {
  //    bool disabled = (m_disabledFaces.find(faceId) != m_disabledFaces.end());
  //    return !disabled;
  //  }
  std::string m_ownPrefix;
  std::map<FaceId, double> m_forwPercMap;

  std::unordered_set<FaceId> m_disabledFaces;

};

}  //fw
}  //nfd

#endif

