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

#ifndef NDN_CONGESTION_TAG_HPP
#define NDN_CONGESTION_TAG_HPP

namespace ndn {

/**
 * Used to simulated both NACKs and congestion marks.
 */
class CongestionTag: public Tag
{
public:
  static size_t getTypeId()
  {
    return 0x1483d925; // md5("CongestionTag")[0:8]
  }

  CongestionTag()
      : m_nackType(-1), m_congMark(0), m_highCongMark(false), m_highCongMarkLocal(false)
  {
  }

  CongestionTag(int8_t nackType, int8_t congMark, bool highCongMark, bool highCongMarkLocal)
      : m_nackType(nackType), m_congMark(congMark), m_highCongMark(highCongMark), m_highCongMarkLocal(
          highCongMarkLocal)
  {
//		std::cout << "Create congestion tag: " << getCongMark() << " ," << getHighCongMark() << ", "
//				<< getHighCongMarkLocal() << "\n";
  }

  void setNackType(int8_t size)
  {
    m_nackType = size;
  }

  int getNackType()
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

private:
  int8_t m_nackType;

  int8_t m_congMark;
  bool m_highCongMark;
  bool m_highCongMarkLocal;

};

} // namespace ndn

#endif // NDN_CONGESTION_TAG_HPP
