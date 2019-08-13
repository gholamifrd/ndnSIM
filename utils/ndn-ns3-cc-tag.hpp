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

#ifndef NDN_NS3_CC_TAG_H
#define NDN_NS3_CC_TAG_H

#include <ns3/tag.h>
#include "../../network/model/tag.h"

namespace ns3 {
class TypeId;
} /* namespace ns3 */

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-fw
 * @brief Packet tag that is used to track queue size */

class Ns3CCTag: public Tag
{
public:
  static TypeId
  GetTypeId(void);

  /**
   * @brief Default constructor
   */
  Ns3CCTag()
      : m_nackType(-1), m_congMark(0), m_highCongMark(false), m_highCongMarkLocal(false)
  {
  }
  ;

  Ns3CCTag(int8_t nackType, int8_t congMark, bool highCongMark, bool highCongMarkLocal)
      : m_nackType(nackType), m_congMark(congMark), m_highCongMark(highCongMark), m_highCongMarkLocal(
          highCongMarkLocal)
  {
  }
  ;

  /**
   * @brief Destructor
   */
  ~Ns3CCTag()
  {
  }

  /**
   * @brief Set queue metric
   */
  void setNackType(int8_t size)
  {
    m_nackType = size;
  }
//
  /**
   * @brief Get value of queue metric
   */
  int8_t getNackType() const
  {
    return m_nackType;
  }

  void setCongMark(int8_t m)
  {
    m_congMark = m;
  }

  void setHighCongMark(bool m)
  {
    m_highCongMark = m;
  }

  void setHighCongMarkLocal(bool m)
  {
    m_highCongMarkLocal = m;
  }

  int8_t getCongMark()
  {
    return m_congMark;
  }

  bool getHighCongMark()
  {
    return m_highCongMark;
  }

  bool getHighCongMarkLocal()
  {
    return m_highCongMarkLocal;
  }

  ////////////////////////////////////////////////////////
  // from ObjectBase
  ////////////////////////////////////////////////////////
  virtual TypeId
  GetInstanceTypeId() const;

  ////////////////////////////////////////////////////////
  // from Tag
  ////////////////////////////////////////////////////////

  virtual uint32_t
  GetSerializedSize() const;

//  Look into .cpp file.
  virtual void
  Serialize(TagBuffer i) const;
 
  virtual void
  Deserialize(TagBuffer i);

  virtual void
  Print(std::ostream& os) const;

private:
  int8_t m_nackType;
  int8_t m_congMark;
  
  bool m_highCongMark;
  bool m_highCongMarkLocal;

};

} // namespace ndn
} // namespace ns3

#endif // NDN_NS3_CC_TAG_H
