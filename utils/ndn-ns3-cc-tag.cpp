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

#include "ndn-ns3-cc-tag.hpp"

namespace ns3 {
namespace ndn {

TypeId Ns3CCTag::GetTypeId()
{
  static TypeId tid = TypeId("ns3::ndn::CCTag").SetParent<Tag>().AddConstructor<Ns3CCTag>();
  return tid;
}

TypeId Ns3CCTag::GetInstanceTypeId() const
{
  return Ns3CCTag::GetTypeId();
}

uint32_t Ns3CCTag::GetSerializedSize() const
{
  return sizeof(uint32_t);
}

void Ns3CCTag::Serialize(TagBuffer i) const
{
  i.WriteU8(m_nackType);
  i.WriteU8(m_congMark);
  i.WriteU8(m_highCongMark);
  i.WriteU8(m_highCongMarkLocal);
}

void Ns3CCTag::Deserialize(TagBuffer i)
{
  m_nackType = i.ReadU8();
  m_congMark = i.ReadU8();
  m_highCongMark = i.ReadU8();
  m_highCongMarkLocal = i.ReadU8();
}

void Ns3CCTag::Print(std::ostream& os) const
{
  os << m_nackType;
}

} // namespace ndn
} // namespace ns3
