/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 MIRKO BANCHI
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
 * Author: Ghada Badawy <gbadawy@rim.com>
 */
#ifndef AMPDU_TAG_H
#define AMPDU_TAG_H

#include "ns3/packet.h"

namespace ns3 {

class Tag;




/**
 * \ingroup wifi
 *
 * The aim of the AmpduTag is to provide means for a MAC to
 * specify that the packet includes AMPDU since this is done in H-SIG and there is no H-SIG representation in ns3
 */
class AmpduTag : public Tag
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  /**
   * Create a AmpduTag with the default =0 no Ampdu
   */
  AmpduTag ();

  /**
   * Set the Ampdu to 1. 
   */
  void SetAmpdu (bool supported);
  void SetNoOfMpdus (uint8_t noofmpdus);


  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual uint32_t GetSerializedSize () const;
  virtual void Print (std::ostream &os) const;

  bool GetAmpdu (void) const;
  uint8_t GetNoOfMpdus (void) const;

private:
  uint8_t m_ampdu;
  uint8_t m_noOfMpdus;
};

} // namespace ns3

#endif /* AMPDU_TAG_H */
