/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 * Author: Ghada Badawy <gbadawy@gmail.com>
 */

#include "yans-wifi-phy.h"
#include "yans-wifi-channel.h"
#include "wifi-mode.h"
#include "wifi-preamble.h"
#include "wifi-phy-state-helper.h"
#include "error-rate-model.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/enum.h"
#include "ns3/pointer.h"
#include "ns3/net-device.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/boolean.h"
#include <cmath>
#include "wifi-mac-header.h"
#include "wifi-mac.h"
#include "regular-wifi-mac.h"
#include "mac-low.h"
#include "ns3/wifi-net-device.h"
#include "ampdu-tag.h"

NS_LOG_COMPONENT_DEFINE ("YansWifiPhy");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (YansWifiPhy)
  ;

TypeId
YansWifiPhy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::YansWifiPhy")
    .SetParent<WifiPhy> ()
    .AddConstructor<YansWifiPhy> ()
    .AddAttribute ("EnergyDetectionThreshold",
                   "The energy of a received signal should be higher than "
                   "this threshold (dbm) to allow the PHY layer to detect the signal.",
                   DoubleValue (-96.0),
                   MakeDoubleAccessor (&YansWifiPhy::SetEdThreshold,
                                       &YansWifiPhy::GetEdThreshold),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("CcaMode1Threshold",
                   "The energy of a received signal should be higher than "
                   "this threshold (dbm) to allow the PHY layer to declare CCA BUSY state",
                   DoubleValue (-99.0),
                   MakeDoubleAccessor (&YansWifiPhy::SetCcaMode1Threshold,
                                       &YansWifiPhy::GetCcaMode1Threshold),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("TxGain",
                   "Transmission gain (dB).",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&YansWifiPhy::SetTxGain,
                                       &YansWifiPhy::GetTxGain),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("RxGain",
                   "Reception gain (dB).",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&YansWifiPhy::SetRxGain,
                                       &YansWifiPhy::GetRxGain),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("TxPowerLevels",
                   "Number of transmission power levels available between "
                   "TxPowerStart and TxPowerEnd included.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&YansWifiPhy::m_nTxPower),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("TxPowerEnd",
                   "Maximum available transmission level (dbm).",
                   DoubleValue (16.0206),
                   MakeDoubleAccessor (&YansWifiPhy::SetTxPowerEnd,
                                       &YansWifiPhy::GetTxPowerEnd),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("TxPowerStart",
                   "Minimum available transmission level (dbm).",
                   DoubleValue (16.0206),
                   MakeDoubleAccessor (&YansWifiPhy::SetTxPowerStart,
                                       &YansWifiPhy::GetTxPowerStart),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("RxNoiseFigure",
                   "Loss (dB) in the Signal-to-Noise-Ratio due to non-idealities in the receiver."
                   " According to Wikipedia (http://en.wikipedia.org/wiki/Noise_figure), this is "
                   "\"the difference in decibels (dB) between"
                   " the noise output of the actual receiver to the noise output of an "
                   " ideal receiver with the same overall gain and bandwidth when the receivers "
                   " are connected to sources at the standard noise temperature T0 (usually 290 K)\"."
                   " For",
                   DoubleValue (7),
                   MakeDoubleAccessor (&YansWifiPhy::SetRxNoiseFigure,
                                       &YansWifiPhy::GetRxNoiseFigure),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("State", "The state of the PHY layer",
                   PointerValue (),
                   MakePointerAccessor (&YansWifiPhy::m_state),
                   MakePointerChecker<WifiPhyStateHelper> ())
    .AddAttribute ("ChannelSwitchDelay",
                   "Delay between two short frames transmitted on different frequencies.",
                   TimeValue (MicroSeconds (250)),
                   MakeTimeAccessor (&YansWifiPhy::m_channelSwitchDelay),
                   MakeTimeChecker ())
    .AddAttribute ("ChannelNumber",
                   "Channel center frequency = Channel starting frequency + 5 MHz * nch",
                   UintegerValue (5),
                   MakeUintegerAccessor (&YansWifiPhy::SetChannelNumber,
                                         &YansWifiPhy::GetChannelNumber),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("ActiveChannels",
                   "The channels where current reception or transmission is happening",
                   UintegerValue (5),
                   MakeUintegerAccessor (&YansWifiPhy::SetActiveChannels,
                                         &YansWifiPhy::GetActiveChannels),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("PrimaryChannel",
                   "The primary channel for this STA/AP",
                   UintegerValue (5),
                   MakeUintegerAccessor (&YansWifiPhy::SetPrimaryChannel,
                                         &YansWifiPhy::GetPrimaryChannel),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("Frequency", "The operating frequency.",
                   UintegerValue (2407),
                   MakeUintegerAccessor (&YansWifiPhy::GetFrequency,
                                        &YansWifiPhy::SetFrequency),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("divider", "The divider.",
                   DoubleValue (1),
                   MakeDoubleAccessor (&YansWifiPhy::GetDivider,
                                        &YansWifiPhy::SetDivider),
                   MakeDoubleChecker<double> ())
	.AddAttribute ("BW", "The operating bandwidth.",
                   UintegerValue (20),
                   MakeUintegerAccessor (&YansWifiPhy::GetBW,
                                        &YansWifiPhy::SetBW),
                   MakeUintegerChecker<uint32_t> ())
	.AddAttribute ("UseNewModel", "Whether or not use the new interference model.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&YansWifiPhy::GetUseNewModel,
                                        &YansWifiPhy::SetUseNewModel),
				   MakeBooleanChecker ())
    .AddAttribute ("CheckAddrAtPhy", "Whether or not to check address in header at phy layer",
                   BooleanValue (true),
                   MakeBooleanAccessor (&YansWifiPhy::GetCheckAddrAtPhy,
                                        &YansWifiPhy::SetCheckAddrAtPhy),
				   MakeBooleanChecker ())
    .AddAttribute ("UseLUTforPER", "Whether or not to use look up table for PER estimation",
                   BooleanValue (true),
                   MakeBooleanAccessor (&YansWifiPhy::GetLUTforPER,
                                        &YansWifiPhy::SetLUTforPER),
				   MakeBooleanChecker ())
	.AddAttribute ("NodeType", "Whether node is STA or AP",
                   UintegerValue (1),
                   MakeUintegerAccessor (&YansWifiPhy::GetNodeType,
                                        &YansWifiPhy::SetNodeType),
                   MakeUintegerChecker<uint32_t> ())
     .AddAttribute ("CodingType", "Whether coding is BCC or LDPC (0 or 1)",
                   UintegerValue (0),
                   MakeUintegerAccessor (&YansWifiPhy::GetCodingType,
                                        &YansWifiPhy::SetCodingType),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Transmitters", "The number of transmitters.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&YansWifiPhy::GetNumberOfTransmitAntennas,
                                        &YansWifiPhy::SetNumberOfTransmitAntennas),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Recievers", "The number of recievers.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&YansWifiPhy::GetNumberOfReceiveAntennas,
                                        &YansWifiPhy::SetNumberOfReceiveAntennas),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("ShortGuardEnabled", "Whether or not short guard interval is enabled.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&YansWifiPhy::GetGuardInterval,
                                        &YansWifiPhy::SetGuardInterval),
                   MakeBooleanChecker ())
    .AddAttribute ("LdpcEnabled", "Whether or not LDPC is enabled.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&YansWifiPhy::GetLdpc,
                                        &YansWifiPhy::SetLdpc),
                   MakeBooleanChecker ())
    .AddAttribute ("STBCEnabled", "Whether or not STBC is enabled.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&YansWifiPhy::GetStbc,
                                        &YansWifiPhy::SetStbc),
                   MakeBooleanChecker ())
    .AddAttribute ("GreenfieldEnabled", "Whether or not STBC is enabled.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&YansWifiPhy::GetGreenfield,
                                        &YansWifiPhy::SetGreenfield),
                   MakeBooleanChecker ())
    .AddAttribute ("ChannelBonding", "Whether 20MHz or 40MHz.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&YansWifiPhy::GetChannelBonding,
                                        &YansWifiPhy::SetChannelBonding),
                   MakeBooleanChecker ())


  ;
  return tid;
}

YansWifiPhy::YansWifiPhy ()
  :  m_channelNumber (5),
    m_endRxEvent (),
    m_channelStartingFrequency (0),
	m_BW (20)

{
  NS_LOG_FUNCTION (this);
  m_random = CreateObject<UniformRandomVariable> ();
  m_state = CreateObject<WifiPhyStateHelper> ();
}

YansWifiPhy::~YansWifiPhy ()
{
  NS_LOG_FUNCTION (this);
}

void
YansWifiPhy::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_channel = 0;
  m_deviceRateSet.clear ();
  m_deviceMcsSet.clear();
  m_device = 0;
  m_mobility = 0;
  m_state = 0;
}

void
YansWifiPhy::ConfigureStandard (enum WifiPhyStandard standard)
{
  NS_LOG_FUNCTION (this << standard);
  switch (standard)
    {
    case WIFI_PHY_STANDARD_80211a:
      Configure80211a ();
      break;
    case WIFI_PHY_STANDARD_80211b:
      Configure80211b ();
      break;
    case WIFI_PHY_STANDARD_80211g:
      Configure80211g ();
      break;
    case WIFI_PHY_STANDARD_80211_10MHZ:
      Configure80211_10Mhz ();
      break;
    case WIFI_PHY_STANDARD_80211_5MHZ:
      Configure80211_5Mhz ();
      break;
    case WIFI_PHY_STANDARD_holland:
      ConfigureHolland ();
      break;
    case WIFI_PHY_STANDARD_80211n_2_4GHZ:
      m_channelStartingFrequency=2400;
      Configure80211n ();
      break;
    case WIFI_PHY_STANDARD_80211n_5GHZ:
      m_channelStartingFrequency=5e3;
      Configure80211n ();
      break;

    default:
      NS_ASSERT (false);
      break;
    }
}


void
YansWifiPhy::SetRxNoiseFigure (double noiseFigureDb)
{
  NS_LOG_FUNCTION (this << noiseFigureDb);
  m_interference.SetNoiseFigure (DbToRatio (noiseFigureDb));
}
void
YansWifiPhy::SetTxPowerStart (double start)
{
  NS_LOG_FUNCTION (this << start);
  m_txPowerBaseDbm = start;
}
void
YansWifiPhy::SetTxPowerEnd (double end)
{
  NS_LOG_FUNCTION (this << end);
  m_txPowerEndDbm = end;
}
void
YansWifiPhy::SetNTxPower (uint32_t n)
{
  NS_LOG_FUNCTION (this << n);
  m_nTxPower = n;
}
void
YansWifiPhy::SetTxGain (double gain)
{
  NS_LOG_FUNCTION (this << gain);
  m_txGainDb = gain;
}
void
YansWifiPhy::SetRxGain (double gain)
{
  NS_LOG_FUNCTION (this << gain);
  m_rxGainDb = gain;
}
void
YansWifiPhy::SetEdThreshold (double threshold)
{
  NS_LOG_FUNCTION (this << threshold);
  m_edThresholdW = DbmToW (threshold);
}
void
YansWifiPhy::SetCcaMode1Threshold (double threshold)
{
  NS_LOG_FUNCTION (this << threshold);
  m_ccaMode1ThresholdW = DbmToW (threshold);
}
void
YansWifiPhy::SetErrorRateModel (Ptr<ErrorRateModel> rate)
{
  m_interference.SetErrorRateModel (rate);
}
void
YansWifiPhy::SetDevice (Ptr<Object> device)
{
  m_device = device;
}
void
YansWifiPhy::SetMobility (Ptr<Object> mobility)
{
  m_mobility = mobility;
}

double
YansWifiPhy::GetRxNoiseFigure (void) const
{
  return RatioToDb (m_interference.GetNoiseFigure ());
}
double
YansWifiPhy::GetTxPowerStart (void) const
{
  return m_txPowerBaseDbm;
}
double
YansWifiPhy::GetTxPowerEnd (void) const
{
  return m_txPowerEndDbm;
}
double
YansWifiPhy::GetTxGain (void) const
{
  return m_txGainDb;
}
double
YansWifiPhy::GetRxGain (void) const
{
  return m_rxGainDb;
}

double
YansWifiPhy::GetEdThreshold (void) const
{
  return WToDbm (m_edThresholdW);
}

double
YansWifiPhy::GetCcaMode1Threshold (void) const
{
  return WToDbm (m_ccaMode1ThresholdW);
}

Ptr<ErrorRateModel>
YansWifiPhy::GetErrorRateModel (void) const
{
  return m_interference.GetErrorRateModel ();
}
Ptr<Object>
YansWifiPhy::GetDevice (void) const
{
  return m_device;
}
Ptr<Object>
YansWifiPhy::GetMobility (void)
{
  return m_mobility;
}

double
YansWifiPhy::CalculateSnr (WifiMode txMode, double ber) const
{
  return m_interference.GetErrorRateModel ()->CalculateSnr (txMode, ber);
}

Ptr<WifiChannel>
YansWifiPhy::GetChannel (void) const
{
  return m_channel;
}
void
YansWifiPhy::SetChannel (Ptr<YansWifiChannel> channel)
{
  m_channel = channel;
  m_channel->Add (this);
}

double
YansWifiPhy::GetDivider (void) const
{
  return m_divide;
}

void
YansWifiPhy::SetDivider (double d) 
{
  m_divide = d;
}

void
YansWifiPhy::SetChannelNumber (uint64_t nch)
{
  if (Simulator::Now () == Seconds (0))
    {
      // this is not channel switch, this is initialization
      NS_LOG_DEBUG ("start at channel " << nch);
      m_channelNumber = nch;
      return;
    }

  NS_ASSERT (!IsStateSwitching ());
  switch (m_state->GetState ())
    {
    case YansWifiPhy::RX:
      NS_LOG_DEBUG ("drop packet because of channel switching while reception");
      m_endRxEvent.Cancel ();
      goto switchChannel;
      break;
    case YansWifiPhy::TX:
      NS_LOG_DEBUG ("channel switching postponed until end of current transmission");
      Simulator::Schedule (GetDelayUntilIdle (), &YansWifiPhy::SetChannelNumber, this, nch);
      break;
    case YansWifiPhy::CCA_BUSY:
    case YansWifiPhy::IDLE:
      goto switchChannel;
      break;
    default:
      NS_ASSERT (false);
      break;
    }

  return;

switchChannel:

  NS_LOG_DEBUG ("switching channel " << m_channelNumber << " -> " << nch);
  m_state->SwitchToChannelSwitching (m_channelSwitchDelay);
  m_interference.EraseEvents ();
  /*
   * Needed here to be able to correctly sensed the medium for the first
   * time after the switching. The actual switching is not performed until
   * after m_channelSwitchDelay. Packets received during the switching
   * state are added to the event list and are employed later to figure
   * out the state of the medium after the switching.
   */
  m_channelNumber = nch;
}

uint64_t
YansWifiPhy::GetChannelNumber () const
{
  return m_channelNumber;
}

uint64_t
YansWifiPhy::GetNumCh_active () const
{
  uint64_t temp_chno = m_activeChannels;
  uint64_t counter = 0;
  while (temp_chno > 0)
	{	        
		temp_chno = temp_chno / 100;
		counter++;
	}
	return counter;
}

uint64_t
YansWifiPhy::GetNumCh_all () const
{
  uint64_t temp_chno = m_channelNumber;
  uint64_t counter = 0;
  while (temp_chno > 0)
	{	        
		temp_chno = temp_chno / 100;
		counter++;
	}
	return counter;
}

uint64_t
YansWifiPhy::GetPosPrimary () const
{
  uint64_t temp_chno = m_channelNumber;
  uint64_t curr, counter = GetNumCh_all() - 1;
  while (temp_chno > 0)
	{	
		curr = temp_chno%100;
		if(curr == m_primaryChannel) {
			return counter;
	    }
					
		temp_chno = temp_chno / 100;
		counter--;
	}
	return counter;
}

void
YansWifiPhy::SetActiveChannels (uint64_t nch)
{
	m_activeChannels  = nch;
}

uint64_t
YansWifiPhy::GetActiveChannels () const
{
  return m_activeChannels;
}

uint64_t
YansWifiPhy::GetActiveChannelsPos () const
{
  std::vector<uint64_t> chnos;
  std::vector<uint64_t> active;
  uint64_t temp_chno = m_channelNumber;
  //std::cout<<m_primaryChannel<<" "<<m_activeChannels<<std::endl;;
  uint64_t curr;
  while (temp_chno > 0)
	{	
		curr = temp_chno%100;
		chnos.push_back(curr);		
		temp_chno = temp_chno / 100;
	}
	temp_chno = m_activeChannels;
	while (temp_chno > 0)
	{	
		curr = temp_chno%100;
		active.push_back(curr);		
		temp_chno = temp_chno / 100;
	}
	uint64_t start=0, end=0, i;
	for(i = 0; i <  chnos.size(); i++) {
		if(chnos.at(i) == active.front()) {
			//start = i;
			end = chnos.size() - 1 - i;
		}
			
		if(chnos.at(i) == active.back()) {
			start = chnos.size() - 1 - i;
			//end = i;
			//break;
		}
	}
	return (10*end + start);
}

void
YansWifiPhy::SetPrimaryChannel (uint64_t nch)
{
	m_primaryChannel  = nch;
}

uint64_t
YansWifiPhy::GetPrimaryChannel () const
{
  return m_primaryChannel;
}

double
YansWifiPhy::GetChannelFrequencyMhz () const
{
  return m_channelStartingFrequency + 5 * GetChannelNumber ();
}

void
YansWifiPhy::SetReceiveOkCallback (RxOkCallback callback)
{
  m_state->SetReceiveOkCallback (callback);
}
void
YansWifiPhy::SetReceiveErrorCallback (RxErrorCallback callback)
{
  m_state->SetReceiveErrorCallback (callback);
}
void
YansWifiPhy::StartReceivePacket (Ptr<Packet> packet,
                                 double *rxPowerDbm,
                                 WifiTxVector txVector,
                                 enum WifiPreamble preamble,
                                 uint8_t packetType)
{
  NS_LOG_FUNCTION (this << packet << rxPowerDbm << txVector.GetMode()<< preamble<< (uint32_t) packetType);
  AmpduTag ampduTag;
  uint64_t i;
  //double *rxPow = new double[GetNumCh_all () + 2];
  for(i=0; i<GetNumCh_all ();i++){
	  *(rxPowerDbm + i) += m_rxGainDb;
	  *(rxPowerDbm + i) = DbmToW (*(rxPowerDbm+i));
  }
  //std::cout<<"there "<<*(rxPowerDbm -1)<<std::endl;
  //rxPowerDbm += m_rxGainDb;
  //double rxPowerW = DbmToW (rxPowerDbm);
  //double *rxPow = new double[1];
  //rxPow[0] = rxPowerW;
 // Time rxDuration = MicroSeconds ((CalculateTxDuration (packet->GetSize (), txVector, preamble)).GetDouble() * m_divide);
 
 
  
  Time rxDuration = (CalculateTxDuration (packet->GetSize (), txVector, preamble,packetType,0));
  //std::cout<<rxDuration<<"    ";
  //rxDuration = NanoSeconds(rxDuration.GetDouble() * m_divide);
  //std::cout<<m_divide<<std::endl;
  
WifiMode txMode=txVector.GetMode();
  Time endRx = Simulator::Now () + rxDuration;

  Ptr<InterferenceHelper::Event> event;
  event = m_interference.Add (packet->GetSize (),
                              txMode,
                              preamble,
                              rxDuration,
                              rxPowerDbm,
		          txVector);  // we need it to calculate duration of HT training symbols

  switch (m_state->GetState ())
    {
    case YansWifiPhy::SWITCHING:
      NS_LOG_DEBUG ("drop packet because of channel switching");
      NotifyRxDrop (packet);
      /*
       * Packets received on the upcoming channel are added to the event list
       * during the switching state. This way the medium can be correctly sensed
       * when the device listens to the channel for the first time after the
       * switching e.g. after channel switching, the channel may be sensed as
       * busy due to other devices' tramissions started before the end of
       * the switching.
       */
      if (endRx > Simulator::Now () + m_state->GetDelayUntilIdle ())
        {
          // that packet will be noise _after_ the completion of the
          // channel switching.
          goto maybeCcaBusy;
        }
      break;
    case YansWifiPhy::RX:
      NS_LOG_DEBUG ("drop packet because already in Rx (power=" <<
                    *(rxPowerDbm) << "W)");
      NotifyRxDrop (packet);
      if (endRx > Simulator::Now () + m_state->GetDelayUntilIdle ())
        {
          // that packet will be noise _after_ the reception of the
          // currently-received packet.
          goto maybeCcaBusy;
        }
      break;
    case YansWifiPhy::TX:
      NS_LOG_DEBUG ("drop packet because already in Tx (power=" <<
                    *(rxPowerDbm) << "W)");
      NotifyRxDrop (packet);
      if (endRx > Simulator::Now () + m_state->GetDelayUntilIdle ())
        {
          // that packet will be noise _after_ the transmission of the
          // currently-transmitted packet.
          goto maybeCcaBusy;
        }
      break;
    case YansWifiPhy::CCA_BUSY:
    case YansWifiPhy::IDLE:
   // std::cout<<*(rxPowerDbm + GetPosPrimary())<<"   "<<m_edThresholdW<<std::endl;
      if (*(rxPowerDbm + GetPosPrimary()) > m_edThresholdW)
        {
			
		  double perPre = m_interference.GetPERPreamble(event, m_coding);
		  if (m_random->GetValue () <= perPre) {
			  NotifyRxDrop (packet);
			  goto maybeCcaBusy;
		  }
		  if (m_addrCheck) {  
			  WifiMacHeader hdr;
			  packet->PeekHeader (hdr);
			  if ((hdr.GetAddr1 () != GetDevice()->GetObject<NetDevice> ()->GetAddress()) && !(hdr.GetAddr1 ().IsGroup ())) {
				Time duration = hdr.GetDuration ();
				Ptr<WifiMac> mac = GetDevice()->GetObject<WifiNetDevice> ()->GetMac();
				Ptr<RegularWifiMac> reg_mac = DynamicCast<RegularWifiMac> (mac); 
				reg_mac->GetMacLow()->DoNavStartNow (duration + rxDuration);
				NotifyRxDrop (packet);
				goto maybeCcaBusy;
			    }
		    }
		    
		    uint64_t activeChannelsPos = *(rxPowerDbm - 2);
			uint64_t a = activeChannelsPos % 10;
			uint64_t b = (activeChannelsPos / 10) % 10;
			if(b-a+1 > *(rxPowerDbm - 1)) {
				NotifyRxDrop (packet);
				goto maybeCcaBusy;
			}
		    
		    //WifiMacHeader hdr;
			  //packet->PeekHeader (hdr);
		    //std::cout<<"HERE"<<(hdr.GetAddr1 ())<<"  "<<(GetDevice()->GetObject<NetDevice> ()->GetAddress())<<std::endl;
		    
		  if (preamble == WIFI_PREAMBLE_NONE && m_mpdusNum ==0 )
               {
                 NS_LOG_DEBUG ("drop packet because no preamble has been received");
                 NotifyRxDrop (packet);
                 goto maybeCcaBusy;
                }
              else if (preamble != WIFI_PREAMBLE_NONE && packet->PeekPacketTag(ampduTag)&&m_mpdusNum == 0)
                {
                  //received the first MPDU in an MPDU
                  m_mpdusNum=ampduTag.GetNoOfMpdus()-1;
                  m_firstMpduStatus=1;
                }
              else if (preamble == WIFI_PREAMBLE_NONE && packet->PeekPacketTag(ampduTag)&&m_mpdusNum > 0)
                {
                   //received the other MPDUs that are part of the A-MPDU
                   if (ampduTag.GetNoOfMpdus() < m_mpdusNum)
                     {
                        NS_LOG_DEBUG ("Missing MPDU from the A-MPDU " << m_mpdusNum- ampduTag.GetNoOfMpdus());
                        m_mpdusNum = ampduTag.GetNoOfMpdus();
                      }
                   else
                     m_mpdusNum--;
                 }
              else if (preamble != WIFI_PREAMBLE_NONE && m_mpdusNum > 0 )
                {  
                  NS_LOG_DEBUG ("Didn't receive the last MPDUs from an A-MPDU " << m_mpdusNum);
                  m_firstMpduStatus=0;
                  m_mpdusNum=0;
                }
              else //not an A-MPDU
				m_firstMpduStatus=0;
		    
          NS_LOG_DEBUG ("sync to signal (power=" << *(rxPowerDbm) << "W)");
          // sync to signal
          m_state->SwitchToRx (rxDuration);
          NS_ASSERT (m_endRxEvent.IsExpired ());
          NotifyRxBegin (packet);
          m_interference.NotifyRxStart ();
          //std::cout<<"here "<<std::endl;
          m_endRxEvent = Simulator::Schedule (rxDuration, &YansWifiPhy::EndReceive, this,
                                              packet,
                                              event);
        }
      else
        {
          NS_LOG_DEBUG ("drop packet because signal power too Small (" <<
                        *(rxPowerDbm) << "<" << m_edThresholdW << ")");
          NotifyRxDrop (packet);
          goto maybeCcaBusy;
        }
      break;
    }

  return;

maybeCcaBusy:
  // We are here because we have received the first bit of a packet and we are
  // not going to be able to synchronize on it
  // In this model, CCA becomes busy when the aggregation of all signals as
  // tracked by the InterferenceHelper class is higher than the CcaBusyThreshold

  Time delayUntilCcaEnd = m_interference.GetEnergyDuration (m_ccaMode1ThresholdW, GetPosPrimary ());
  //std::cout<<"sdf "<<delayUntilCcaEnd<<std::endl;
  if (!delayUntilCcaEnd.IsZero ())
    {
      m_state->SwitchMaybeToCcaBusy (delayUntilCcaEnd);
    }
}

void
YansWifiPhy::SendPacket (Ptr<const Packet> packet,  WifiTxVector txVector, WifiPreamble preamble, uint8_t packetType)
{
	/*
	
	if(m_divide == 2) {
	    mode1 = GetOfdmRate52MbpsBW20MHz ();
	    txVector.SetMode(mode1);
  }*/
  NS_LOG_FUNCTION (this << packet << txVector.GetMode() << preamble << (uint32_t)txVector.GetTxPowerLevel()<< (uint32_t) packetType);
  /* Transmission can happen if:
   *  - we are syncing on a packet. It is the responsability of the
   *    MAC layer to avoid doing this but the PHY does nothing to
   *    prevent it.
   *  - we are idle
   */
  NS_ASSERT (!m_state->IsStateTx () && !m_state->IsStateSwitching ());
  //std::cout<<packet->GetSize ()<<std::endl;
  Time txDuration = CalculateTxDuration (packet->GetSize (), txVector, preamble, packetType, 0);
  //txDuration = NanoSeconds(txDuration.GetDouble() * m_divide);
  if (m_state->IsStateRx ())
    {
      m_endRxEvent.Cancel ();
      m_interference.NotifyRxEnd ();
    }
  NotifyTxBegin (packet);
  uint32_t dataRate500KbpsUnits = txVector.GetMode().GetDataRate () * txVector.GetNss() / 500000;
  bool isShortPreamble = (WIFI_PREAMBLE_SHORT == preamble);
  NotifyMonitorSniffTx (packet, (uint16_t)GetChannelFrequencyMhz (), GetChannelNumber (), dataRate500KbpsUnits, isShortPreamble, txVector.GetTxPowerLevel());
  m_state->SwitchToTx (txDuration, packet, txVector.GetMode(), preamble,  txVector.GetTxPowerLevel());
  if(m_newModel) {
	m_channel->Send_NewModel (this, packet, GetPowerDbm ( txVector.GetTxPowerLevel()) + m_txGainDb, txVector, preamble, packetType);
  }
  else {
	m_channel->Send (this, packet, GetPowerDbm ( txVector.GetTxPowerLevel()) + m_txGainDb, txVector, preamble, packetType);
  }
}

uint32_t
YansWifiPhy::GetNModes (void) const
{
  return m_deviceRateSet.size ();
}
WifiMode
YansWifiPhy::GetMode (uint32_t mode) const
{
  return m_deviceRateSet[mode];
}
uint32_t
YansWifiPhy::GetNTxPower (void) const
{
  return m_nTxPower;
}

void
YansWifiPhy::Configure80211a (void)
{
  NS_LOG_FUNCTION (this);
  m_channelStartingFrequency = 5e3; // 5.000 GHz

  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate6Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate9Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate12Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate18Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate24Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate36Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate48Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate54Mbps ());
}


void
YansWifiPhy::Configure80211b (void)
{
  NS_LOG_FUNCTION (this);
  m_channelStartingFrequency = 2407; // 2.407 GHz

  m_deviceRateSet.push_back (WifiPhy::GetDsssRate1Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetDsssRate2Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetDsssRate5_5Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetDsssRate11Mbps ());
}

void
YansWifiPhy::Configure80211g (void)
{
  NS_LOG_FUNCTION (this);
  m_channelStartingFrequency = 2407; // 2.407 GHz

  m_deviceRateSet.push_back (WifiPhy::GetDsssRate1Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetDsssRate2Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetDsssRate5_5Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate6Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate9Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetDsssRate11Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate12Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate18Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate24Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate36Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate48Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate54Mbps ());
}

void
YansWifiPhy::Configure80211_10Mhz (void)
{
  NS_LOG_FUNCTION (this);
  m_channelStartingFrequency = 5e3; // 5.000 GHz, suppose 802.11a

  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate3MbpsBW10MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate4_5MbpsBW10MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate6MbpsBW10MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate9MbpsBW10MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate12MbpsBW10MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate18MbpsBW10MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate24MbpsBW10MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate27MbpsBW10MHz ());
}

void
YansWifiPhy::Configure80211_5Mhz (void)
{
  NS_LOG_FUNCTION (this);
  m_channelStartingFrequency = 5e3; // 5.000 GHz, suppose 802.11a

  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate1_5MbpsBW5MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate2_25MbpsBW5MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate3MbpsBW5MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate4_5MbpsBW5MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate6MbpsBW5MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate9MbpsBW5MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate12MbpsBW5MHz ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate13_5MbpsBW5MHz ());
}

void
YansWifiPhy::ConfigureHolland (void)
{
  NS_LOG_FUNCTION (this);
  m_channelStartingFrequency = 5e3; // 5.000 GHz
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate6Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate12Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate18Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate36Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetOfdmRate54Mbps ());
}

void
YansWifiPhy::RegisterListener (WifiPhyListener *listener)
{
  m_state->RegisterListener (listener);
}

bool
YansWifiPhy::IsStateCcaBusy (void)
{
  return m_state->IsStateCcaBusy ();
}

bool
YansWifiPhy::IsStateIdle (void)
{
  return m_state->IsStateIdle ();
}
bool
YansWifiPhy::IsStateBusy (void)
{
  return m_state->IsStateBusy ();
}
bool
YansWifiPhy::IsStateRx (void)
{
  return m_state->IsStateRx ();
}
bool
YansWifiPhy::IsStateTx (void)
{
  return m_state->IsStateTx ();
}
bool
YansWifiPhy::IsStateSwitching (void)
{
  return m_state->IsStateSwitching ();
}

Time
YansWifiPhy::GetStateDuration (void)
{
  return m_state->GetStateDuration ();
}
Time
YansWifiPhy::GetDelayUntilIdle (void)
{
  return m_state->GetDelayUntilIdle ();
}

Time
YansWifiPhy::GetLastRxStartTime (void) const
{
  return m_state->GetLastRxStartTime ();
}

double
YansWifiPhy::DbToRatio (double dB) const
{
  double ratio = std::pow (10.0, dB / 10.0);
  return ratio;
}

double
YansWifiPhy::DbmToW (double dBm) const
{
  double mW = std::pow (10.0, dBm / 10.0);
  return mW / 1000.0;
}

double
YansWifiPhy::WToDbm (double w) const
{
  return 10.0 * std::log10 (w * 1000.0);
}

double
YansWifiPhy::RatioToDb (double ratio) const
{
  return 10.0 * std::log10 (ratio);
}

double
YansWifiPhy::GetEdThresholdW (void) const
{
  return m_edThresholdW;
}

double
YansWifiPhy::GetPowerDbm (uint8_t power) const
{
  NS_ASSERT (m_txPowerBaseDbm <= m_txPowerEndDbm);
  NS_ASSERT (m_nTxPower > 0);
  double dbm;
  if (m_nTxPower > 1)
    {
      dbm = m_txPowerBaseDbm + power * (m_txPowerEndDbm - m_txPowerBaseDbm) / (m_nTxPower - 1);
    }
  else
    {
      NS_ASSERT_MSG (m_txPowerBaseDbm == m_txPowerEndDbm, "cannot have TxPowerEnd != TxPowerStart with TxPowerLevels == 1");
      dbm = m_txPowerBaseDbm;
    }
  return dbm;
}

void
YansWifiPhy::EndReceive (Ptr<Packet> packet, Ptr<InterferenceHelper::Event> event)
{
  NS_LOG_FUNCTION (this << packet << event);
  NS_ASSERT (IsStateRx ());
  NS_ASSERT (event->GetEndTime () == Simulator::Now ());

  struct InterferenceHelper::SnrPer snrPer;
  snrPer = m_interference.CalculateSnrPer (event, m_useLUT, m_coding);
  m_interference.NotifyRxEnd ();
  //std::cout<<packet->GetSize ()<<" "<<snrPer.per<<std::endl;
  NS_LOG_DEBUG ("mode=" << (event->GetPayloadMode ().GetDataRate ()) <<
                ", snr=" << snrPer.snr << ", per=" << snrPer.per << ", size=" << packet->GetSize ());
                
  if (m_firstMpduStatus==3)
    {
       //the first packet was not received correctly so no MPDUs from the AMPDU can be received
       NotifyRxDrop (packet);
      m_state->SwitchFromRxEndError (packet, snrPer.snr);
    }              
  
  else if (m_random->GetValue () > snrPer.per)
    {
      NotifyRxEnd (packet);
      uint32_t dataRate500KbpsUnits = event->GetPayloadMode ().GetDataRate () * event->GetTxVector().GetNss()/ 500000;
      bool isShortPreamble = (WIFI_PREAMBLE_SHORT == event->GetPreambleType ());
      double signalDbm = RatioToDb (*(event->GetRxPowerW ())) + 30;
      double noiseDbm = RatioToDb (*(event->GetRxPowerW ()) / snrPer.snr) - GetRxNoiseFigure () + 30;
      NotifyMonitorSniffRx (packet, (uint16_t)GetChannelFrequencyMhz (), GetChannelNumber (), dataRate500KbpsUnits, isShortPreamble, signalDbm, noiseDbm);
      m_state->SwitchFromRxEndOk (packet, snrPer.snr, event->GetPayloadMode (), event->GetPreambleType ());
      if (m_firstMpduStatus==1)
        //if this packet is the first mpdu in an AMPDU flag that the first packet was received correctly
        m_firstMpduStatus=2;
    }
  else
    {
      /* failure. */
      NotifyRxDrop (packet);
      m_state->SwitchFromRxEndError (packet, snrPer.snr);
      //if this packet is the first mpdu in an AMPDU flag that the first packet was not received correctly
      if (m_firstMpduStatus==1)
        m_firstMpduStatus=3;
    }
}

int64_t
YansWifiPhy::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_random->SetStream (stream);
  return 1;
}

void
YansWifiPhy::SetFrequency (uint32_t freq)
{
  m_channelStartingFrequency = freq;
}

void
YansWifiPhy::SetUseNewModel (bool use)
{
  m_newModel = use;
}

void
YansWifiPhy::SetCheckAddrAtPhy (bool use)
{
  m_addrCheck = use;
}

void
YansWifiPhy::SetLUTforPER (bool use)
{
  m_useLUT = use;
}

void
YansWifiPhy::SetBW (uint32_t bw)
{
  m_BW = bw;
}

void
YansWifiPhy::SetNodeType(uint32_t nt)
{
  m_nt = nt;
}

void
YansWifiPhy::SetCodingType(uint32_t coding)
{
  m_coding = coding;
}

void
YansWifiPhy::SetNumberOfTransmitAntennas (uint32_t tx)
{
  m_numberOfTransmitters = tx;
}
void
YansWifiPhy::SetNumberOfReceiveAntennas (uint32_t rx)
{
  m_numberOfReceivers = rx;
}

void
YansWifiPhy::SetLdpc (bool Ldpc)
{
  m_ldpc = Ldpc;
}

void
YansWifiPhy::SetStbc (bool stbc)
{
  m_stbc = stbc;
}

void
YansWifiPhy::SetGreenfield (bool greenfield)
{
  m_greenfield = greenfield;
}
bool
YansWifiPhy::GetGuardInterval (void) const
{
  return m_guardInterval;
}
void
YansWifiPhy::SetGuardInterval (bool guardInterval)
{
  m_guardInterval = guardInterval;
}

uint32_t
YansWifiPhy::GetFrequency (void) const
{
  return m_channelStartingFrequency;
}

bool
YansWifiPhy::GetUseNewModel (void) const
{
  return m_newModel;
}

bool
YansWifiPhy::GetCheckAddrAtPhy (void) const
{
  return m_addrCheck;
}

bool
YansWifiPhy::GetLUTforPER (void) const
{
  return m_useLUT;
}

uint32_t
YansWifiPhy::GetBW (void) const
{
  return m_BW;
}

uint32_t
YansWifiPhy::GetNodeType (void) const
{
  return m_nt;
}

uint32_t
YansWifiPhy::GetCodingType (void) const
{
  return m_coding;
}

uint32_t
YansWifiPhy::GetNumberOfTransmitAntennas (void) const
{
  return m_numberOfTransmitters;
}
uint32_t
YansWifiPhy::GetNumberOfReceiveAntennas (void) const
{
  return m_numberOfReceivers;
}

bool
YansWifiPhy::GetLdpc (void) const
{
  return m_ldpc;
}
bool
YansWifiPhy::GetStbc (void) const
{
  return m_stbc;
}

bool
YansWifiPhy::GetGreenfield (void) const
{
  return m_greenfield;
}

bool
YansWifiPhy::GetChannelBonding(void) const
{
  return m_channelBonding;
}

void
YansWifiPhy::SetChannelBonding(bool channelbonding) 
{
  m_channelBonding= channelbonding;
}

void
YansWifiPhy::Configure80211n (void)
{
  NS_LOG_FUNCTION (this);
   //problem with these is that Ack Timeout is smaller that the TX Duration of a packet using these rates
  /*m_deviceRateSet.push_back (WifiPhy::GetDsssRate1Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetDsssRate2Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetDsssRate5_5Mbps ());*/
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate6Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetDsssRate11Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate12Mbps ());
  m_deviceRateSet.push_back (WifiPhy::GetErpOfdmRate24Mbps ());
  m_bssMembershipSelectorSet.push_back(HT_PHY);
  for (uint8_t i=0; i <8; i++)
    {
      m_deviceMcsSet.push_back(i);
    }

}
uint32_t
YansWifiPhy::GetNBssMembershipSelectors (void) const
{
  return  m_bssMembershipSelectorSet.size ();
}
uint32_t
YansWifiPhy::GetBssMembershipSelector (uint32_t selector) const
{
  return  m_bssMembershipSelectorSet[selector];
}
WifiModeList
YansWifiPhy::GetMembershipSelectorModes(uint32_t selector)
{
  uint32_t id=GetBssMembershipSelector(selector);
  WifiModeList supportedmodes;
  if (id == HT_PHY)
  {
    //mandatory MCS 0 to 7
     supportedmodes.push_back (WifiPhy::GetOfdmRate6_5MbpsBW20MHz ());
     supportedmodes.push_back (WifiPhy::GetOfdmRate13MbpsBW20MHz ());
     supportedmodes.push_back (WifiPhy::GetOfdmRate19_5MbpsBW20MHz ());
     supportedmodes.push_back (WifiPhy::GetOfdmRate26MbpsBW20MHz ());
     supportedmodes.push_back (WifiPhy::GetOfdmRate39MbpsBW20MHz ());
     supportedmodes.push_back (WifiPhy::GetOfdmRate52MbpsBW20MHz ());
     supportedmodes.push_back (WifiPhy::GetOfdmRate58_5MbpsBW20MHz ());
     supportedmodes.push_back (WifiPhy::GetOfdmRate65MbpsBW20MHz ());
  }
  return supportedmodes;
}
uint8_t
YansWifiPhy::GetNMcs (void) const
{
  return  m_deviceMcsSet.size ();
}
uint8_t
YansWifiPhy::GetMcs (uint8_t mcs) const
{
  return  m_deviceMcsSet[mcs];
}
uint32_t 
YansWifiPhy::WifiModeToMcs (WifiMode mode)
{
    uint32_t mcs = 0;
   if (mode.GetUniqueName() == "OfdmRate135MbpsBW40MHzShGi" || mode.GetUniqueName() == "OfdmRate65MbpsBW20MHzShGi" )
     {
             mcs=6;
     }
  else
    {
     switch (mode.GetDataRate())
       {
         case 6500000:
         case 7200000:
         case 13500000:
         case 15000000:
           mcs=0;
           break;
         case 13000000:
         case 14400000:
         case 27000000:
         case 30000000:
           mcs=1;
           break;
         case 19500000:
         case 21700000:
         case 40500000:
         case 45000000:
           mcs=2;
           break;
         case 26000000:
         case 28900000:
         case 54000000:
         case 60000000:
           mcs=3;
           break;
         case 39000000:
         case 43300000:
         case 81000000:
         case 90000000:        
           mcs=4;
           break;
         case 52000000:
         case 57800000:
         case 108000000:
         case 120000000:
           mcs=5;
           break; 
         case 58500000:
         case 121500000:
           mcs=6;
           break;
         case 65000000:
         case 72200000:
         case 135000000:
         case 150000000:
           mcs=7;
           break;     
       }
    }
  return mcs;
}
WifiMode
YansWifiPhy::McsToWifiMode (uint8_t mcs)
{
   WifiMode mode;
   switch (mcs)
     { 
       case 7:
          if (!GetGuardInterval() && !GetChannelBonding())
           {
              mode =  WifiPhy::GetOfdmRate65MbpsBW20MHz ();
            }
         else if(GetGuardInterval() && !GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate72_2MbpsBW20MHz ();
            }
          else if (!GetGuardInterval() && GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate135MbpsBW40MHz ();
            }
          else
            {
              mode = WifiPhy::GetOfdmRate150MbpsBW40MHz ();
            }
          break;
       case 6:
          if (!GetGuardInterval() && !GetChannelBonding())
           {
              mode = WifiPhy::GetOfdmRate58_5MbpsBW20MHz ();
 
            }
         else if(GetGuardInterval() && !GetChannelBonding())
            {
              mode =  WifiPhy::GetOfdmRate65MbpsBW20MHzShGi ();
       
            }
          else if (!GetGuardInterval() && GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate121_5MbpsBW40MHz ();
     
            }
          else
            {
              mode= WifiPhy::GetOfdmRate135MbpsBW40MHzShGi ();
          
            }
          break;
       case 5:
          if (!GetGuardInterval() && !GetChannelBonding())
           {
              mode = WifiPhy::GetOfdmRate52MbpsBW20MHz ();
  
            }
         else if(GetGuardInterval() && !GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate57_8MbpsBW20MHz ();
            }
          else if (!GetGuardInterval() && GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate108MbpsBW40MHz ();
     
            }
          else
            {
              mode = WifiPhy::GetOfdmRate120MbpsBW40MHz ();
       
            }
          break;
       case 4:
          if (!GetGuardInterval() && !GetChannelBonding())
           {
              mode = WifiPhy::GetOfdmRate39MbpsBW20MHz ();
            }
         else if(GetGuardInterval() && !GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate43_3MbpsBW20MHz ();
            }
          else if (!GetGuardInterval() && GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate81MbpsBW40MHz ();
  
            }
          else
            {
              mode = WifiPhy::GetOfdmRate90MbpsBW40MHz ();
         
            }
          break;
       case 3:
          if (!GetGuardInterval() && !GetChannelBonding())
           {
              mode =  WifiPhy::GetOfdmRate26MbpsBW20MHz ();
  
            }
         else if(GetGuardInterval() && !GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate28_9MbpsBW20MHz ();
      
            }
          else if (!GetGuardInterval() && GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate54MbpsBW40MHz ();
     
            }
          else
            {
              mode = WifiPhy::GetOfdmRate60MbpsBW40MHz ();
            }
          break;
       case 2:
          if (!GetGuardInterval() && !GetChannelBonding())
           {
              mode = WifiPhy::GetOfdmRate19_5MbpsBW20MHz ();
 
            }
         else if(GetGuardInterval() && !GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate21_7MbpsBW20MHz ();
     
            }
          else if (!GetGuardInterval() && GetChannelBonding())
            {
              mode =  WifiPhy::GetOfdmRate40_5MbpsBW40MHz ();
  
            }
          else
            {
              mode = WifiPhy::GetOfdmRate45MbpsBW40MHz ();
           
            }
          break;
       case 1:
          if (!GetGuardInterval() && !GetChannelBonding())
           {
            mode = WifiPhy::GetOfdmRate13MbpsBW20MHz ();
  
            }
         else if(GetGuardInterval() && !GetChannelBonding())
            {
              mode =  WifiPhy::GetOfdmRate14_4MbpsBW20MHz ();
            }
          else if (!GetGuardInterval() && GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate27MbpsBW40MHz ();
     
            }
          else
            {
              mode = WifiPhy::GetOfdmRate30MbpsBW40MHz ();
            }
          break;
       case 0:
       default:
         if (!GetGuardInterval() && !GetChannelBonding())
           {
              mode = WifiPhy::GetOfdmRate6_5MbpsBW20MHz ();
              
            }
         else if(GetGuardInterval() && !GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate7_2MbpsBW20MHz ();
            }
          else if (!GetGuardInterval() && GetChannelBonding())
            {
              mode = WifiPhy::GetOfdmRate13_5MbpsBW40MHz ();
 
            }
          else
            {
              mode = WifiPhy::GetOfdmRate15MbpsBW40MHz ();
            }
         break;
        }
 return mode;
}
} // namespace ns3
