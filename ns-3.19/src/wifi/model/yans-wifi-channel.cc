/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006,2007 INRIA
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
 * Author: Mathieu Lacage, <mathieu.lacage@sophia.inria.fr>
 */
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/mobility-model.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/object-factory.h"
#include "yans-wifi-channel.h"
#include "yans-wifi-phy.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include <cmath>

NS_LOG_COMPONENT_DEFINE ("YansWifiChannel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (YansWifiChannel)
  ;

TypeId
YansWifiChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::YansWifiChannel")
    .SetParent<WifiChannel> ()
    .AddConstructor<YansWifiChannel> ()
    .AddAttribute ("PropagationLossModel", "A pointer to the propagation loss model attached to this channel.",
                   PointerValue (),
                   MakePointerAccessor (&YansWifiChannel::m_loss),
                   MakePointerChecker<PropagationLossModel> ())
    .AddAttribute ("PropagationDelayModel", "A pointer to the propagation delay model attached to this channel.",
                   PointerValue (),
                   MakePointerAccessor (&YansWifiChannel::m_delay),
                   MakePointerChecker<PropagationDelayModel> ())
  ;
  return tid;
}

YansWifiChannel::YansWifiChannel ()
{
}
YansWifiChannel::~YansWifiChannel ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_phyList.clear ();
}

void
YansWifiChannel::SetPropagationLossModel (Ptr<PropagationLossModel> loss)
{
  m_loss = loss;
}
void
YansWifiChannel::SetPropagationDelayModel (Ptr<PropagationDelayModel> delay)
{
  m_delay = delay;
}


void
YansWifiChannel::Send (Ptr<YansWifiPhy> sender, Ptr<const Packet> packet, double txPowerDbm,
                       WifiTxVector txVector, WifiPreamble preamble, uint8_t packetType) const
{
  Ptr<MobilityModel> senderMobility = sender->GetMobility ()->GetObject<MobilityModel> ();
  NS_ASSERT (senderMobility != 0);
  uint32_t j = 0;
  for (PhyList::const_iterator i = m_phyList.begin (); i != m_phyList.end (); i++, j++)
    {
      if (sender != (*i))
        {
          // For now don't account for inter channel interference
          if ((*i)->GetChannelNumber () != sender->GetChannelNumber ())
            {
              continue;
            }

          Ptr<MobilityModel> receiverMobility = (*i)->GetMobility ()->GetObject<MobilityModel> ();
          Time delay = m_delay->GetDelay (senderMobility, receiverMobility);
          
          double rxPowerDbm = 0.0;
          
          //when loss model is Outdoor Model
          
          if (dynamic_cast<OutdoorPropagationLossModel*>(PeekPointer(m_loss)) != NULL) {
			  if(sender->GetNodeType() == 1 && (*i)->GetNodeType() == 1) {
				  //rxPowerDbm = m_loss->CalcRxPower_STA_STA (txPowerDbm, senderMobility, receiverMobility);
				  rxPowerDbm = dynamic_cast<OutdoorPropagationLossModel*>(PeekPointer(m_loss))->DoCalcRxPower_STA_STA (txPowerDbm, senderMobility, receiverMobility);
			  }
			  else if (sender->GetNodeType() == 2 && (*i)->GetNodeType() == 2) {
				  //rxPowerDbm = m_loss->CalcRxPower_AP_AP (txPowerDbm, senderMobility, receiverMobility);
				  rxPowerDbm = dynamic_cast<OutdoorPropagationLossModel*>(PeekPointer(m_loss))->DoCalcRxPower_AP_AP (txPowerDbm, senderMobility, receiverMobility);
			  }
			  else {
				  //rxPowerDbm = m_loss->CalcRxPower_STA_AP (txPowerDbm, senderMobility, receiverMobility);
				  rxPowerDbm = dynamic_cast<OutdoorPropagationLossModel*>(PeekPointer(m_loss))->DoCalcRxPower_STA_AP (txPowerDbm, senderMobility, receiverMobility);
			  }
		  }
		  else {
			  rxPowerDbm = m_loss->CalcRxPower (txPowerDbm, senderMobility, receiverMobility);
		  }
          
          //double *rxPow = new double[3];
           double *rxPow = new double[4];

          memset(rxPow, 0, (4)*sizeof(*rxPow));
          rxPow[0] = packetType;
          rxPow[2] = 1;
          rxPow[1] = sender->GetActiveChannelsPos();
          rxPow+=3;
          
		  //rxPow[1] = 1;
		  //rxPow[0] = sender->GetActiveChannelsPos();
		  //rxPow+=2;
		  
		  
          rxPow[0] = rxPowerDbm;
          NS_LOG_DEBUG ("propagation: txPower=" << txPowerDbm << "dbm, rxPower=" << rxPowerDbm << "dbm, " <<
                        "distance=" << senderMobility->GetDistanceFrom (receiverMobility) << "m, delay=" << delay);
          Ptr<Packet> copy = packet->Copy ();
          Ptr<Object> dstNetDevice = m_phyList[j]->GetDevice ();
          uint32_t dstNode;
          if (dstNetDevice == 0)
            {
              dstNode = 0xffffffff;
            }
          else
            {
              dstNode = dstNetDevice->GetObject<NetDevice> ()->GetNode ()->GetId ();
            }
          Simulator::ScheduleWithContext (dstNode,
                                          delay, &YansWifiChannel::Receive, this,
                                          j, copy, rxPow, txVector, preamble);
        }
    }
}

void
YansWifiChannel::Send_NewModel (Ptr<YansWifiPhy> sender, Ptr<const Packet> packet, double txPowerDbm,
                       WifiTxVector txVector, WifiPreamble preamble, uint8_t packetType)
{
  Ptr<MobilityModel> senderMobility = sender->GetMobility ()->GetObject<MobilityModel> ();
  NS_ASSERT (senderMobility != 0);
    

  uint32_t quantum = 20; // MHz  
  uint64_t k, l=0, numch_sender, numch_i;	
  uint64_t temp_chno = sender->GetActiveChannels ();
  //numch_sender = sender->GetBW () / quantum;
  numch_sender = sender->GetNumCh_active ();
  uint64_t sender_chnos[4];
  uint64_t i_chnos[4], high[4], low[4];
  bool send = false;

  for(k = numch_sender; k > 0; k--)
	{	        
		sender_chnos[k-1] = temp_chno % 100;
		temp_chno = temp_chno / 100;
	}
	    
  uint64_t interferingChannels[4 * 2];
  uint64_t first = sender_chnos[0];
  uint64_t last = sender_chnos[numch_sender - 1];

  for(k = 1; k <= numch_sender; k++) 
  {
	interferingChannels[l++] = last + k;
	interferingChannels[l++] = first - k;
  }
  
  uint32_t j = 0, overlap = 0, overlap_adj = 0, indx_high = 0, indx_low = 0;
  double attenuation;
  for (PhyList::const_iterator i = m_phyList.begin (); i != m_phyList.end (); i++, j++)
    {
      if (sender != (*i) && sender->GetFrequency () == (*i)->GetFrequency ())
        {
		 
		  temp_chno = (*i)->GetChannelNumber ();
		  //numch_i = (*i)->GetBW () / quantum;
		  numch_i = (*i)->GetNumCh_all();
		  
		  double *rxPow  = new double[numch_i+3];
		  memset(rxPow, 0, (numch_i+3)*sizeof(*rxPow));
		  rxPow[0] = packetType;
		  rxPow[2] = numch_i;
		  rxPow[1] = sender->GetActiveChannelsPos ();
		  rxPow += 3;
		  
		  //double *rxPow  = new double[numch_i+2];
		  //memset(rxPow, 0, (numch_i+2)*sizeof(*rxPow));
		  //rxPow[1] = numch_i;
		  //rxPow[0] = sender->GetActiveChannelsPos ();
		  //rxPow += 2;
		  
		  
		  //std::cout<<"here "<<(uint64_t)(*(rxPow-1))<<std::endl;
		  Ptr<MobilityModel> receiverMobility = (*i)->GetMobility ()->GetObject<MobilityModel> ();
		  Time delay = m_delay->GetDelay (senderMobility, receiverMobility);
		  
		  for(k = numch_i; k > 0; k--)
			{
				i_chnos[k-1] = temp_chno % 100;
				temp_chno = temp_chno / 100;
				for(l = 0; l < numch_sender; l++) 
				{
					if (sender_chnos[l] == i_chnos[k-1] )
					{
						overlap++;
					}
				}
				for(l = 0; l < numch_sender*2; l++) 
				{
					if (interferingChannels[l] == i_chnos[k-1] )
					{
						overlap_adj++;
						if(i_chnos[k-1] > last) 
						{
							high[indx_high++] = i_chnos[k-1] - last;

						}
						else 
						{
							low[indx_low++] = first - i_chnos[k-1];

						}
						
					}
				}
			//}	
			
			  if (overlap != 0 || overlap_adj != 0) {
				  
				  send = true;
				  double rxPowerDbm = 0;
				  double attenuation_factor = 0;
				  //Ptr<MobilityModel> receiverMobility = (*i)->GetMobility ()->GetObject<MobilityModel> ();
				  //Time delay = m_delay->GetDelay (senderMobility, receiverMobility);
				  
				  //double rxPower_minus_attn = m_loss->CalcRxPower (txPowerDbm, senderMobility, receiverMobility);
				  double rxPower_minus_attn = 0.0;
				  
				  if (dynamic_cast<OutdoorPropagationLossModel*>(PeekPointer(m_loss)) != NULL) {
					  if(sender->GetNodeType() == 1 && (*i)->GetNodeType() == 1) {
						  //rxPowerDbm = m_loss->CalcRxPower_STA_STA (txPowerDbm, senderMobility, receiverMobility);
						  rxPower_minus_attn = dynamic_cast<OutdoorPropagationLossModel*>(PeekPointer(m_loss))->DoCalcRxPower_STA_STA (txPowerDbm, senderMobility, receiverMobility);
					  }
					  else if (sender->GetNodeType() == 2 && (*i)->GetNodeType() == 2) {
						  //rxPowerDbm = m_loss->CalcRxPower_AP_AP (txPowerDbm, senderMobility, receiverMobility);
						  rxPower_minus_attn = dynamic_cast<OutdoorPropagationLossModel*>(PeekPointer(m_loss))->DoCalcRxPower_STA_STA (txPowerDbm, senderMobility, receiverMobility);
					  }
					  else {
						  //rxPowerDbm = m_loss->CalcRxPower_STA_AP (txPowerDbm, senderMobility, receiverMobility);
						  rxPower_minus_attn = dynamic_cast<OutdoorPropagationLossModel*>(PeekPointer(m_loss))->DoCalcRxPower_STA_STA (txPowerDbm, senderMobility, receiverMobility);
					  }
				  }
				  else {
					  rxPower_minus_attn = m_loss->CalcRxPower (txPowerDbm, senderMobility, receiverMobility);
				  }
				  
				  
				  if(overlap !=0 )
				  {
					attenuation_factor += ((overlap * 1.0) / numch_sender);
					//rxPowerDbm += rxPower_minus_attn + (10 * std::log10 (overlap * 1.0 / numch_sender));
					//std::cout<<"co-ch = "<<overlap<<"\n";
				  }
				  if(overlap_adj != 0 )
				  {
					if (quantum == 20 && sender->GetFrequency () == 2400) 
					{
						attenuation = CalculateAttenuation20MHzQuantum2_4Ghz (overlap_adj, high, indx_high, low, indx_low, sender->GetBW());
					}
					else if (quantum == 20 && sender->GetFrequency () == 5000) 
					{
						attenuation = CalculateAttenuation20MHzQuantum5Ghz (overlap_adj, high, indx_high, low, indx_low, sender->GetBW());
					}
					attenuation_factor += (std::pow(10.0,(attenuation /10.0)));
					//rxPowerDbm += rxPower_minus_attn + attenuation;
					//std::cout<<"adj-ch = "<<attenuation<<"\n";
				  }
				  //std::cout<<"attenuation = "<<(10 * std::log10 (attenuation_factor))<<"\n";
				  rxPowerDbm += rxPower_minus_attn + (10 * std::log10 (attenuation_factor));
				  rxPow[k-1] = rxPowerDbm;
			  }
			  overlap = 0;
			  overlap_adj = 0;
			  indx_high = 0;
			  indx_low = 0;
			}
		      if(send) {
				  //NS_LOG_DEBUG ("propagation: txPower=" << txPowerDbm << "dbm, rxPower=" << rxPowerDbm << "dbm, " <<
				  //				"distance=" << senderMobility->GetDistanceFrom (receiverMobility) << "m, delay=" << delay);
				  Ptr<Packet> copy = packet->Copy ();
				  Ptr<Object> dstNetDevice = m_phyList[j]->GetDevice ();
				  uint32_t dstNode;
				  if (dstNetDevice == 0)
					{
					  dstNode = 0xffffffff;
					}
				  else
					{
					  dstNode = dstNetDevice->GetObject<NetDevice> ()->GetNode ()->GetId ();
					}
				  Simulator::ScheduleWithContext (dstNode,
												  delay, &YansWifiChannel::Receive, this,
												  j, copy, rxPow, txVector, preamble);
			  }
			  send = false;
			//}
			/*
			overlap = 0;
			overlap_adj = 0;
			indx_high = 0;
			indx_low = 0;
			*/
        }
    }
}


double
YansWifiChannel::CalculateAttenuation20MHzQuantum2_4Ghz (uint32_t overlap, uint64_t high[], uint32_t indx_high, uint64_t low[], uint32_t indx_low, uint32_t BW) 
{
	double attenuation = 0.0;

	if (BW == 20) 
	{
		if (overlap == 1)
			attenuation =  -23.77; //dB
		else if (overlap == 2)
			attenuation = -20.77; //dB
	}
	else if (BW == 40)
	{
		if (overlap == 1)
		{
			if ((indx_high == 1 && high[0] == 1) || (indx_low == 1 && low[0] == 1))
				attenuation = -25.02; //dB
			else if ((indx_high == 1 && high[0] == 2) || (indx_low == 1 && low[0] == 2))
				attenuation = -36.99; //dB	
		}
		else if (overlap == 2) 
		{
			if ((indx_high == 2 && high[0] == 1 && high[1] == 2) || (indx_low == 2 && low[0] == 1 && low[1] == 2))
				attenuation = -24.75; //dB
			else if (indx_high == 1 and indx_low == 1 && high[0] == 1 && low[0] == 1) 
				attenuation = -22.02; //dB
		}
	}
	else if (BW == 80) {
		if (overlap == 1) 
		{
			if ((indx_high == 1 && high[0] == 1) || (indx_low == 1 && low[0] == 1))
				attenuation = -26.88; //dB
			else if ((indx_high == 1 && high[0] == 2) || (indx_low == 1 && low[0] == 2))
				attenuation = -31.79; //dB;
			else if ((indx_high == 1 && high[0] == 3) || (indx_low == 1 && low[0] == 3))
				attenuation = -36.73; //dB
			else if ((indx_high == 1 && high[0] == 4) || (indx_low == 1 && low[0] == 4))
				attenuation = -43.01; //dB
		}
		else if (overlap == 2)
		{
			if ((indx_high == 2 && high[0] == 1 && high[1] == 2) || (indx_low == 2 && low[0] == 1 && low[1] == 2))
				attenuation = -25.67; //dB
			else if ((indx_high == 2 && high[0] == 2 && high[1] == 3) || (indx_low == 2 && low[0] == 2 && low[1] == 3))
				attenuation = -30.58; //dB
			else if ((indx_high == 2 && high[0] == 3 && high[1] == 4) || (indx_low == 2 && low[0] == 3 && low[1] == 4))
				attenuation = -35.81; //dB
		}	
		else if (overlap == 3)
		{
			if ((indx_high == 3 && high[0] == 1 && high[1] == 2 && high[2] == 3) || (indx_low == 3 && low[0] == 1 && low[1] == 2 && low[2] == 3))
				attenuation = -25.34; //dB
			else if ((indx_high == 3 && high[0] == 2 && high[1] == 3 && high[2] == 4) || (indx_low == 3 && low[0] == 2 && low[1] == 3 && low[2] == 4))
				attenuation = -30.34; //dB
		}
		else if (overlap == 4)
		{
			attenuation = -25.27; //dB
		}
	}

	return attenuation;
}

double
YansWifiChannel::CalculateAttenuation20MHzQuantum5Ghz (uint32_t overlap, uint64_t high[], uint32_t indx_high, uint64_t low[], uint32_t indx_low, uint32_t BW) 
{
	double attenuation = 0.0;

	if (BW == 20) 
	{
		if (overlap == 1)
			attenuation =  -23.72; //dB
		else if (overlap == 2)
			attenuation = -20.72; //dB
	}
	else if (BW == 40)
	{
		if (overlap == 1)
		{
			if ((indx_high == 1 && high[0] == 1) || (indx_low == 1 && low[0] == 1))
				attenuation = -25.02; //dB
			else if ((indx_high == 1 && high[0] == 2) || (indx_low == 1 && low[0] == 2))
				attenuation = -35.61; //dB	
		}
		else if (overlap == 2) 
		{
			if ((indx_high == 2 && high[0] == 1 && high[1] == 2) || (indx_low == 2 && low[0] == 1 && low[1] == 2))
				attenuation = -24.65; //dB
			else if (indx_high == 1 and indx_low == 1 && high[0] == 1 && low[0] == 1) 
				attenuation = -22.02; //dB
		}
	}
	else if (BW == 80) {
		if (overlap == 1) 
		{
			if ((indx_high == 1 && high[0] == 1) || (indx_low == 1 && low[0] == 1))
				attenuation = -26.88; //dB
			else if ((indx_high == 1 && high[0] == 2) || (indx_low == 1 && low[0] == 2))
				attenuation = -31.79; //dB;
			else if ((indx_high == 1 && high[0] == 3) || (indx_low == 1 && low[0] == 3))
				attenuation = -36.73; //dB
			else if ((indx_high == 1 && high[0] == 4) || (indx_low == 1 && low[0] == 4))
				attenuation = -43.01; //dB
		}
		else if (overlap == 2)
		{
			if ((indx_high == 2 && high[0] == 1 && high[1] == 2) || (indx_low == 2 && low[0] == 1 && low[1] == 2))
				attenuation = -25.67; //dB
			else if ((indx_high == 2 && high[0] == 2 && high[1] == 3) || (indx_low == 2 && low[0] == 2 && low[1] == 3))
				attenuation = -30.58; //dB
			else if ((indx_high == 2 && high[0] == 3 && high[1] == 4) || (indx_low == 2 && low[0] == 3 && low[1] == 4))
				attenuation = -35.81; //dB
		}	
		else if (overlap == 3)
		{
			if ((indx_high == 3 && high[0] == 1 && high[1] == 2 && high[2] == 3) || (indx_low == 3 && low[0] == 1 && low[1] == 2 && low[2] == 3))
				attenuation = -25.34; //dB
			else if ((indx_high == 3 && high[0] == 2 && high[1] == 3 && high[2] == 4) || (indx_low == 3 && low[0] == 2 && low[1] == 3 && low[2] == 4))
				attenuation = -30.34; //dB
		}
		else if (overlap == 4)
		{
			attenuation = -25.27; //dB
		}
	}

	return attenuation;
}

void
YansWifiChannel::Receive (uint32_t i, Ptr<Packet> packet, double *rxPowerDbm,
                          WifiTxVector txVector, WifiPreamble preamble) const
{
  m_phyList[i]->StartReceivePacket (packet, rxPowerDbm, txVector, preamble, *(rxPowerDbm-3));
}

uint32_t
YansWifiChannel::GetNDevices (void) const
{
  return m_phyList.size ();
}
Ptr<NetDevice>
YansWifiChannel::GetDevice (uint32_t i) const
{
  return m_phyList[i]->GetDevice ()->GetObject<NetDevice> ();
}

void
YansWifiChannel::Add (Ptr<YansWifiPhy> phy)
{
  m_phyList.push_back (phy);
}

int64_t
YansWifiChannel::AssignStreams (int64_t stream)
{
  int64_t currentStream = stream;
  currentStream += m_loss->AssignStreams (stream);
  return (currentStream - stream);
}

} // namespace ns3
