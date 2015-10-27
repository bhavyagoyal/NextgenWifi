/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013
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
 * Author: Ghada Badawy <gbadawy@gmail.com>
 */
#ifndef MPDU_AGGREGATOR_H
#define MPDU_AGGREGATOR_H

#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/object.h"

#include "ampdu-subframe-header.h"

#include <list>

namespace ns3 {

class WifiMacHeader;

/**
 * \brief Abstract class that concrete mpdu aggregators have to implement
 * \ingroup wifi
 */
class MpduAggregator : public Object
{
public:
  typedef std::list<std::pair<Ptr<Packet>, AmpduSubframeHeader> > DeaggregatedMpdus;
  typedef std::list<std::pair<Ptr<Packet>, AmpduSubframeHeader> >::const_iterator DeaggregatedMpdusCI;

  static TypeId GetTypeId (void);
  /* Adds <i>packet</i> to <i>aggregatedPacket</i>. In concrete aggregator's implementation is
   * specified how and if <i>packet</i> can be added to <i>aggregatedPacket</i>. If <i>packet</i>
   * can be added returns true, false otherwise.
   */
  virtual bool Aggregate (Ptr<const Packet> packet, Ptr<Packet> aggregatedPacket) = 0;
  virtual bool AggregateAtFront (Ptr<const Packet> packet, Ptr<Packet> aggregatedPacket)=0;
  virtual void AddHeaderAndPad (Ptr<Packet> packet,bool last)=0;
  virtual bool CanBeAggregated (uint32_t packetSize, Ptr<Packet> aggregatedPacket, uint8_t blockAckSize)=0;
  virtual uint32_t GetAmpduSize (void)=0;
  virtual uint32_t CalculatePadding (Ptr<const Packet> packet)=0;
  static DeaggregatedMpdus Deaggregate (Ptr<Packet> aggregatedPacket);
};

}  // namespace ns3

#endif /* MPDU_AGGREGATOR_H */
