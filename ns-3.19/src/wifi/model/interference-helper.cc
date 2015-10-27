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
 */
#include "interference-helper.h"
#include "wifi-phy.h"
#include "yans-wifi-phy.h"
#include "error-rate-model.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <cmath>

#define SSTR( x ) dynamic_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

NS_LOG_COMPONENT_DEFINE ("InterferenceHelper");

namespace ns3 {

/****************************************************************
 *       Phy event class
 ****************************************************************/

InterferenceHelper::Event::Event (uint32_t size, WifiMode payloadMode,
                                  enum WifiPreamble preamble,
                                  Time duration, double *rxPower, WifiTxVector txVector)
  : m_size (size),
    m_payloadMode (payloadMode),
    m_preamble (preamble),
    m_startTime (Simulator::Now ()),
    m_endTime (m_startTime + duration),
    m_rxPowerW (rxPower),
    m_txVector (txVector)
{
}
InterferenceHelper::Event::~Event ()
{
}

Time
InterferenceHelper::Event::GetDuration (void) const
{
  return m_endTime - m_startTime;
}
Time
InterferenceHelper::Event::GetStartTime (void) const
{
  return m_startTime;
}
Time
InterferenceHelper::Event::GetEndTime (void) const
{
  return m_endTime;
}
double
*InterferenceHelper::Event::GetRxPowerW (void) const
{
  return m_rxPowerW;
}
uint32_t
InterferenceHelper::Event::GetSize (void) const
{
  return m_size;
}
WifiMode
InterferenceHelper::Event::GetPayloadMode (void) const
{
  return m_payloadMode;
}
enum WifiPreamble
InterferenceHelper::Event::GetPreambleType (void) const
{
  return m_preamble;
}

WifiTxVector
InterferenceHelper::Event::GetTxVector (void) const
{
  return m_txVector;
}


/****************************************************************
 *       Class which records SNIR change events for a
 *       short period of time.
 ****************************************************************/

InterferenceHelper::NiChange::NiChange (Time time, double delta)
  : m_time (time),
    m_delta (delta)
{
}
Time
InterferenceHelper::NiChange::GetTime (void) const
{
  return m_time;
}
double
InterferenceHelper::NiChange::GetDelta (void) const
{
  return m_delta;
}
bool
InterferenceHelper::NiChange::operator < (const InterferenceHelper::NiChange& o) const
{
  return (m_time < o.m_time);
}

/****************************************************************
 *       The actual InterferenceHelper
 ****************************************************************/

InterferenceHelper::InterferenceHelper ()
  : m_errorRateModel (0),
    m_firstPowers (0),
    m_rxing (false)
{
}
InterferenceHelper::~InterferenceHelper ()
{
  EraseEvents ();
  m_errorRateModel = 0;
}

Ptr<InterferenceHelper::Event>
InterferenceHelper::Add (uint32_t size, WifiMode payloadMode,
                         enum WifiPreamble preamble,
                         Time duration, double *rxPowerW, WifiTxVector txVector)
{
  Ptr<InterferenceHelper::Event> event;

  event = Create<InterferenceHelper::Event> (size,
                                             payloadMode,
                                             preamble,
                                             duration,
                                             rxPowerW,
                                             txVector);
  AppendEvent (event);
  return event;
}


void
InterferenceHelper::SetNoiseFigure (double value)
{
  m_noiseFigure = value;
}

double
InterferenceHelper::GetNoiseFigure (void) const
{
  return m_noiseFigure;
}

void
InterferenceHelper::SetErrorRateModel (Ptr<ErrorRateModel> rate)
{
  m_errorRateModel = rate;
}

Ptr<ErrorRateModel>
InterferenceHelper::GetErrorRateModel (void) const
{
  return m_errorRateModel;
}

Time
InterferenceHelper::GetEnergyDuration (double energyW, uint64_t primary)
{ 
	
  m_niChanges = m_niChangesList.at(primary);
  //std::cout<<"ha"<<primary<<std::endl;
  Time now = Simulator::Now ();
  double noiseInterferenceW = 0.0;
  Time end = now;
  noiseInterferenceW = m_firstPowers.at(primary);
  for (NiChanges::const_iterator i = m_niChanges.begin (); i != m_niChanges.end (); i++)
    {
      noiseInterferenceW += i->GetDelta ();
      end = i->GetTime ();
      if (end < now)
        {
          continue;
        }
      if (noiseInterferenceW < energyW)
        {
          break;
        }
    }
  return end > now ? end - now : MicroSeconds (0);
}


Time
InterferenceHelper::GetEnergyDurationFromT (double energyW, uint64_t pos_channel, Time now)
{ 
	//std::cout<<"ha"<<pos_channel<<std::endl;
  if(m_niChangesList.size() == 0) {
	  return Simulator::Now ();
	  
  }
  m_niChanges = m_niChangesList.at(pos_channel);
  
  //Time now = Simulator::Now ();
  double noiseInterferenceW = 0.0;
  Time end = now;
  noiseInterferenceW = m_firstPowers.at(pos_channel);
  for (NiChanges::const_iterator i = m_niChanges.begin (); i != m_niChanges.end (); i++)
    {
      noiseInterferenceW += i->GetDelta ();
      end = i->GetTime ();
      if (end < now)
        {
          continue;
        }
      if (noiseInterferenceW < energyW)
        {
          break;
        }
    }
  return end > now ? end - now : MicroSeconds (0);
}

void
InterferenceHelper::AppendEvent (Ptr<InterferenceHelper::Event> event)
{
  Time now = Simulator::Now ();
  
  uint64_t num_ch = (uint64_t)(*(event->GetRxPowerW ()-1));
  uint64_t j;
  //std::cout<<"here1"<<num_ch<<std::endl;
  if (m_niChangesList.size() == 0) {
	  m_niChangesList.resize(num_ch);
  }
  if (m_firstPowers.size() == 0) {
	  m_firstPowers.resize(num_ch);
  }//std::cout<<"here"<<m_niChangesList.size()<<std::endl;
  for(j = 0; j < num_ch; j++) {
	  
	  m_niChanges = m_niChangesList.at(j);
		
	  if (!m_rxing)
		{
		  NiChanges::iterator nowIterator = GetPosition (now);
		  for (NiChanges::iterator i = m_niChanges.begin (); i != nowIterator; i++)
			{
			  m_firstPowers[j] = (m_firstPowers.at(j) + i->GetDelta ());
			}
		  m_niChanges.erase (m_niChanges.begin (), nowIterator);
		  m_niChanges.insert (m_niChanges.begin (), NiChange (event->GetStartTime (), *(event->GetRxPowerW ()+j)));
		  //std::cout<<"here"<<m_niChangesList.at(j).size()<<std::endl;
		}
	  else
		{
		  AddNiChangeEvent (NiChange (event->GetStartTime (), *(event->GetRxPowerW ()+j)));
		}
	  AddNiChangeEvent (NiChange (event->GetEndTime (), -1.0 * (*(event->GetRxPowerW ()+j))));
	  m_niChangesList[j] = (m_niChanges);
	  
  }

}


double
InterferenceHelper::CalculateSnr (double signal, double noiseInterference, WifiMode mode) const
{
  // thermal noise at 290K in J/s = W
  static const double BOLTZMANN = 1.3803e-23;
  // Nt is the power of thermal noise in W
  double Nt = BOLTZMANN * 290.0 * mode.GetBandwidth ();
  // receiver noise Floor (W) which accounts for thermal noise and non-idealities of the receiver
  double noiseFloor = m_noiseFigure * Nt;
  double noise = noiseFloor + noiseInterference;
  double snr = signal / noise;
  return snr;
}

double
InterferenceHelper::CalculateNoiseInterferenceW (Ptr<InterferenceHelper::Event> event, NiChanges *ni) const
{
  double noiseInterference = 0.0;
  //NS_ASSERT (m_rxing);
  uint64_t j;
  uint64_t activeChannelsPos = (uint64_t)(*(event->GetRxPowerW() - 2));
  //std::cout<<activeChannelsPos<<"  "<<(*(event->GetRxPowerW() - 1))<<std::endl;
  uint64_t a = activeChannelsPos % 10;
  uint64_t b = (activeChannelsPos / 10) % 10;
  
  NiChange min(event->GetEndTime (), 0.0);
  int K[b - a + 1];
  bool run_loop = true;
  
  for (j = a; j <= b; j++) {
	 K[j] = 1;
	 noiseInterference += m_firstPowers.at(j);
  }
  
  while (true) {
	  //std::cout<<a<<"  "<<b<<std::endl;
	  //std::cout<<"size "<<m_niChangesList.at(0).size()<<std::endl;
	  for (j = a; j <= b; j++) {
		  NiChange nch = (m_niChangesList.at(j)).at(K[j]);
		  if (event->GetEndTime () == nch.GetTime () && *(event->GetRxPowerW ()+j) == -nch.GetDelta ()) {
			  run_loop = false;
			  break;
		  }
		  if (nch.GetTime() <= min.GetTime()) {
			  min = nch;
		  }
		  
	  }
	  if(!run_loop) {
		  break;
	  }
	  double power = 0.0;
	  
	  for (j = a; j <= b; j++) {
		  NiChange nch = (m_niChangesList.at(j)).at(K[j]);
		  if (nch.GetTime() == min.GetTime()) {
			  power += nch.GetDelta();
			  K[j]++;
		  }
		  
	  }
	  NiChange ins(min.GetTime(), power);
	  ni->push_back (ins);
	  min = NiChange(event->GetEndTime (), 0.0);
  
  }
  /*
  for (NiChanges::const_iterator i = m_niChanges.begin () + 1; i != m_niChanges.end (); i++)
    {
      if ((event->GetEndTime () == i->GetTime ()) && *(event->GetRxPowerW ()) == -i->GetDelta ())
        {
          break;
        }
      ni->push_back (*i);
    }
    */
  ni->insert (ni->begin (), NiChange (event->GetStartTime (), noiseInterference));
  ni->push_back (NiChange (event->GetEndTime (), 0));
  return noiseInterference;
}

double
InterferenceHelper::CalculateChunkSuccessRate (double snir, Time duration, WifiMode mode) const
{
  if (duration == NanoSeconds (0))
    {
      return 1.0;
    }
  uint32_t rate = mode.GetPhyRate ();
  uint64_t nbits = (uint64_t)(rate * duration.GetSeconds ());
  double csr = m_errorRateModel->GetChunkSuccessRate (mode, snir, (uint32_t)nbits);
  return csr;
}

uint32_t 
InterferenceHelper::WifiModeToMcs (WifiMode mode) const
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

double InterferenceHelper::getPer(uint32_t coding, uint32_t PL0, uint32_t chunk_mcs, double snr) const {
          
          double snrdb = 10.0 * std::log10 (snr);
          int temp = snrdb*10.0;
          snrdb = temp/10.0;
          std::string firstlevel;
          if(coding == 0)
			firstlevel = "BCC";
		  else
			firstlevel = "LDPC";
		
          std::string secondlevel;		
          if(PL0 == 32)		
			secondlevel = "32";
		  else
			secondlevel = "1458";
			
		  std::string thirdlevel = SSTR(chunk_mcs);
		  
		  std::string filename;
		  
		  filename  = firstlevel + secondlevel + "mcs" + thirdlevel+".txt";
          
          std::ifstream in(filename.c_str());

		  std::string s;
         //for performance
          s.reserve(100);    
		  int i = 0;
		  double per = 2.0;
          while(std::getline(in,s)) {
			   std::stringstream linestream(s);
			   std::string data1, data2;
			   
			   std::getline(linestream, data1, '\t');
			   std::getline(linestream, data2, '\t');
			   
			   double snri = atof (data1.c_str());
			   double peri = atof (data2.c_str());
			   
			   if(i==0) {
				   if(snrdb < snri) {
					   per = 1.0;
					   break;
				   }
			   }
			   if(snri == snrdb) {
				   per = peri;
				   break;
			   }
			   i++;					   
		  }
		  if(per == 2.0)
			per = 0.0;
	   return per;
}

double
InterferenceHelper::CalculatePer_LUT (Ptr<const InterferenceHelper::Event> event, NiChanges *ni, uint32_t coding) const
{  
  uint64_t activeChannelsPos = (uint64_t)(*(event->GetRxPowerW () - 2));
  uint64_t i;
  uint64_t a = activeChannelsPos % 10;
  uint64_t b = (activeChannelsPos / 10) % 10;
  double powerW = 0.0;
  for(i = a; i <= b; i++) {
	  powerW += (*(event->GetRxPowerW () + i));
  }
	
	
  double psr = 1.0; /* Packet Success Rate */
  NiChanges::iterator j = ni->begin ();
  Time previous = (*j).GetTime ();
  WifiMode payloadMode = event->GetPayloadMode ();
  WifiPreamble preamble = event->GetPreambleType ();
 WifiMode MfHeaderMode ;
 if (preamble==WIFI_PREAMBLE_HT_MF)
   {
    MfHeaderMode = WifiPhy::GetMFPlcpHeaderMode (payloadMode, preamble); //return L-SIG mode

   }
  WifiMode headerMode = WifiPhy::GetPlcpHeaderMode (payloadMode, preamble);
  Time plcpHeaderStart = (*j).GetTime () + MicroSeconds (WifiPhy::GetPlcpPreambleDurationMicroSeconds (payloadMode, preamble)); //packet start time+ preamble
  Time plcpHsigHeaderStart=plcpHeaderStart+ MicroSeconds (WifiPhy::GetPlcpHeaderDurationMicroSeconds (payloadMode, preamble));//packet start time+ preamble+L SIG
  Time plcpHtTrainingSymbolsStart = plcpHsigHeaderStart + MicroSeconds (WifiPhy::GetPlcpHtSigHeaderDurationMicroSeconds (payloadMode, preamble));//packet start time+ preamble+L SIG+HT SIG
  Time plcpPayloadStart =plcpHtTrainingSymbolsStart + MicroSeconds (WifiPhy::GetPlcpHtTrainingSymbolDurationMicroSeconds (payloadMode, preamble,event->GetTxVector())); //packet start time+ preamble+L SIG+HT SIG+Training
  double noiseInterferenceW = (*j).GetDelta ();
  //double powerW = *(event->GetRxPowerW ());
  j++;
    
    
  uint32_t PL0 = 0;  
  if(coding == 0) {
	  if(event->GetSize() < 400)
		  PL0 = 32;
	  else 
		  PL0 = 1458;
  }
  else
	  PL0 = 1458;
   
  double total_symbols = (event->GetDuration()).GetMicroSeconds() / 4.0;
  while (ni->end () != j)
    {
      Time current = (*j).GetTime ();
      NS_ASSERT (current >= previous);
      //Case 1: Both prev and curr point to the payload
      if (previous >= plcpPayloadStart)
        {
          
          uint32_t chunk_mcs = WifiModeToMcs (payloadMode);
          double chunk_symbols = (current - previous).GetMicroSeconds() / 4.0;
          double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
                                                          noiseInterferenceW,
                                                          payloadMode));
          double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
          if(psr_temp < psr)
			psr = psr_temp;
          
        }
      //Case 2: previous is before payload
      else if (previous >= plcpHtTrainingSymbolsStart)
        {
          //Case 2a: current is after payload
          if (current >= plcpPayloadStart)
            { 
               //Case 2ai and 2aii: All formats
             
              uint32_t chunk_mcs = WifiModeToMcs (payloadMode);
			  double chunk_symbols = (current - plcpPayloadStart).GetMicroSeconds() / 4.0;
			  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
															  noiseInterferenceW,
															  payloadMode));
			  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
			  if(psr_temp < psr)
				psr = psr_temp;
			}
				  
        }
      //Case 3: previous is in HT-SIG: Non HT will not enter here since it didn't enter in the last two and they are all the same for non HT
      else if (previous >=plcpHsigHeaderStart)
        {
          //Case 3a: cuurent after payload start
          if (current >=plcpPayloadStart)
             {
                  
                  uint32_t chunk_mcs = WifiModeToMcs (payloadMode);
				  double chunk_symbols = (current - plcpPayloadStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  payloadMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                             
                    
                 
                                                
                  chunk_mcs = WifiModeToMcs (headerMode);
				  chunk_symbols = (plcpHtTrainingSymbolsStart - previous).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;
              }
          //case 3b: current after HT training symbols start
          else if (current >=plcpHtTrainingSymbolsStart)
             {
               double chunk_mcs = WifiModeToMcs (headerMode);
			   double chunk_symbols = (plcpHtTrainingSymbolsStart - previous).GetMicroSeconds() / 4.0;
			   double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                      
                
                   
             }
         //Case 3c: current is with previous in HT sig
         else
            {
                  
               double chunk_mcs = WifiModeToMcs (headerMode);                                    
               double chunk_symbols = (current- previous).GetMicroSeconds() / 4.0;
			   double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                       
                 
                   
            }
      }
      //Case 4: previous in L-SIG: GF will not reach here because it will execute the previous if and exit
      else if (previous >= plcpHeaderStart)
        {
          //Case 4a: current after payload start  
          if (current >=plcpPayloadStart)
             {
                   
                   double chunk_mcs = WifiModeToMcs (payloadMode);                                    
				   double chunk_symbols = (current- plcpPayloadStart).GetMicroSeconds() / 4.0;
				   double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																	  noiseInterferenceW,
																	  payloadMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                                 
                    //Case 4ai: Non HT format (No HT-SIG or Training Symbols)
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT) //plcpHtTrainingSymbolsStart==plcpHeaderStart)
                {
                    
                  chunk_mcs = WifiModeToMcs (headerMode);
				  chunk_symbols = (plcpPayloadStart - previous).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                            
                }

               else{
                    
                  chunk_mcs = WifiModeToMcs (headerMode);
				  chunk_symbols = (plcpHtTrainingSymbolsStart - plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                      
                                                      
                                                      
                    
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart - previous).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                   
                                                      
                 }
              }
           //Case 4b: current in HT training symbol. non HT will not come here since it went in previous if or if the previous ifis not true this will be not true        
          else if (current >=plcpHtTrainingSymbolsStart)
             {
                
                  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (plcpHtTrainingSymbolsStart - plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                   
                                                  
                                                  
                
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart - previous).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                        
                                                   
              }
          //Case 4c: current in H sig.non HT will not come here since it went in previous if or if the previous ifis not true this will be not true
          else if (current >=plcpHsigHeaderStart)
             {
				 
				  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (current - plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                   
                                                  
                                                  
                
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart - previous).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp; 

             }
         //Case 4d: Current with prev in L SIG
         else 
            {
                //Case 4di: Non HT format (No HT-SIG or Training Symbols)
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT) //plcpHtTrainingSymbolsStart==plcpHeaderStart)
                {
                    
                  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (current - previous).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                            
                                                
                }
               else
                {
                
                  double chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  double chunk_symbols = (current - previous).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                      
                                                 
                }
            }
        }
      //Case 5: previous is in the preamble works for all cases
      else
        {
          if (current >= plcpPayloadStart)
            {
              //for all
              
                  double chunk_mcs = WifiModeToMcs (payloadMode);
				  double chunk_symbols = (current - plcpPayloadStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  payloadMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                 
                                                
             
               // Non HT format (No HT-SIG or Training Symbols)
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT) {
                 
                  chunk_mcs = WifiModeToMcs (headerMode);
				  chunk_symbols = (plcpPayloadStart - plcpHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                    
                                                    
               }                                 
              else {
              // Greenfield or Mixed format
                
                  chunk_mcs = WifiModeToMcs (headerMode);
				  chunk_symbols = (plcpHtTrainingSymbolsStart - plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                  
			  }
              if (preamble == WIFI_PREAMBLE_HT_MF) {
                 
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart-plcpHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                  
                                                   
			  }            
            }
          else if (current >=plcpHtTrainingSymbolsStart )
          { 
              // Non HT format will not come here since it will execute prev if
              // Greenfield or Mixed format
                
                  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (plcpHtTrainingSymbolsStart - plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                   
                                                  
              if (preamble == WIFI_PREAMBLE_HT_MF)
                  
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart-plcpHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                        
                                                         
           }
          //non HT will not come here     
          else if (current >=plcpHsigHeaderStart)
             { 
                
                  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (current- plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                  
                                                  
                if  (preamble != WIFI_PREAMBLE_HT_GF)
                 {
                     
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart-plcpHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                  
                                                       
                  }          
             }
          // GF will not come here
          else if (current >= plcpHeaderStart)
            {
               if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT)
                 {
                 
                  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (current - plcpHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                  
                                                    
                 }
              else
                 {
              
                                              
                  double chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  double chunk_symbols = (current - plcpHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                                                
                                               
                   }
            }
        }

      noiseInterferenceW += (*j).GetDelta ();
      previous = (*j).GetTime ();
      j++;
    }

  double final_per = 1 - psr;
  return final_per;
}

double
InterferenceHelper::GetPERPreamble (Ptr<InterferenceHelper::Event> event, uint32_t coding) 
{  
	
  NiChanges ni1;
  CalculateNoiseInterferenceW (event, &ni1);
  NiChanges *ni = &ni1;
  uint64_t activeChannelsPos = (uint64_t)(*(event->GetRxPowerW () - 2));
  uint64_t i;
  uint64_t a = activeChannelsPos % 10;
  uint64_t b = (activeChannelsPos / 10) % 10;
  double powerW = 0.0;
  for(i = a; i <= b; i++) {
	  powerW += (*(event->GetRxPowerW () + i));
  }
	
	
  double psr = 1.0; /* Packet Success Rate */
  NiChanges::iterator j = ni->begin ();
  Time previous = (*j).GetTime ();
  Time starting = previous;
  WifiMode payloadMode = event->GetPayloadMode ();
  WifiPreamble preamble = event->GetPreambleType ();
 WifiMode MfHeaderMode ;
 if (preamble==WIFI_PREAMBLE_HT_MF)
   {
    MfHeaderMode = WifiPhy::GetMFPlcpHeaderMode (payloadMode, preamble); //return L-SIG mode

   }
  WifiMode headerMode = WifiPhy::GetPlcpHeaderMode (payloadMode, preamble);
  Time plcpHeaderStart = (*j).GetTime () + MicroSeconds (WifiPhy::GetPlcpPreambleDurationMicroSeconds (payloadMode, preamble)); //packet start time+ preamble
  Time plcpHsigHeaderStart=plcpHeaderStart+ MicroSeconds (WifiPhy::GetPlcpHeaderDurationMicroSeconds (payloadMode, preamble));//packet start time+ preamble+L SIG
  Time plcpHtTrainingSymbolsStart = plcpHsigHeaderStart + MicroSeconds (WifiPhy::GetPlcpHtSigHeaderDurationMicroSeconds (payloadMode, preamble));//packet start time+ preamble+L SIG+HT SIG
  Time plcpPayloadStart =plcpHtTrainingSymbolsStart + MicroSeconds (WifiPhy::GetPlcpHtTrainingSymbolDurationMicroSeconds (payloadMode, preamble,event->GetTxVector())); //packet start time+ preamble+L SIG+HT SIG+Training
  double noiseInterferenceW = (*j).GetDelta ();
  //double powerW = *(event->GetRxPowerW ());
  j++;
    
    
  uint32_t PL0 = 0;  
  if(coding == 0) {
	  if(event->GetSize() < 400)
		  PL0 = 32;
	  else 
		  PL0 = 1458;
  }
  else
	  PL0 = 1458;
   
  double total_symbols = (event->GetDuration()).GetMicroSeconds() / 4.0;
  while (ni->end () != j)
    {
      Time current = (*j).GetTime ();
      NS_ASSERT (current >= previous);
      //Case 1: Both prev and curr point to the payload
      if (previous >= plcpPayloadStart)
        {
          
          uint32_t chunk_mcs = WifiModeToMcs (payloadMode);
          double chunk_symbols = (current - previous).GetMicroSeconds() / 4.0;
          double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
                                                          noiseInterferenceW,
                                                          payloadMode));
          double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
          if(psr_temp < psr)
			psr = psr_temp;
          
        }
      //Case 2: previous is before payload
      else if (previous >= plcpHtTrainingSymbolsStart)
        {
          //Case 2a: current is after payload
          if (current >= plcpPayloadStart)
            { 
               //Case 2ai and 2aii: All formats
             
              uint32_t chunk_mcs = WifiModeToMcs (payloadMode);
			  double chunk_symbols = (current - plcpPayloadStart).GetMicroSeconds() / 4.0;
			  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
															  noiseInterferenceW,
															  payloadMode));
			  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
			  if(psr_temp < psr)
				psr = psr_temp;
			}
				  
        }
      //Case 3: previous is in HT-SIG: Non HT will not enter here since it didn't enter in the last two and they are all the same for non HT
      else if (previous >=plcpHsigHeaderStart)
        {
          //Case 3a: cuurent after payload start
          if (current >=plcpPayloadStart)
             {
                  
                  uint32_t chunk_mcs = WifiModeToMcs (payloadMode);
				  double chunk_symbols = (current - plcpPayloadStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  payloadMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                             
                    
                 
                                                
                  chunk_mcs = WifiModeToMcs (headerMode);
				  chunk_symbols = (plcpHtTrainingSymbolsStart - previous).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;
              }
          //case 3b: current after HT training symbols start
          else if (current >=plcpHtTrainingSymbolsStart)
             {
               double chunk_mcs = WifiModeToMcs (headerMode);
			   double chunk_symbols = (plcpHtTrainingSymbolsStart - previous).GetMicroSeconds() / 4.0;
			   double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                      
                
                   
             }
         //Case 3c: current is with previous in HT sig
         else
            {
                  
               double chunk_mcs = WifiModeToMcs (headerMode);                                    
               double chunk_symbols = (current- previous).GetMicroSeconds() / 4.0;
			   double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                       
                 
                   
            }
      }
      //Case 4: previous in L-SIG: GF will not reach here because it will execute the previous if and exit
      else if (previous >= plcpHeaderStart)
        {
          //Case 4a: current after payload start  
          if (current >=plcpPayloadStart)
             {
                   
                   double chunk_mcs = WifiModeToMcs (payloadMode);                                    
				   double chunk_symbols = (current- plcpPayloadStart).GetMicroSeconds() / 4.0;
				   double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																	  noiseInterferenceW,
																	  payloadMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                                 
                    //Case 4ai: Non HT format (No HT-SIG or Training Symbols)
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT) //plcpHtTrainingSymbolsStart==plcpHeaderStart)
                {
                    
                  chunk_mcs = WifiModeToMcs (headerMode);
				  chunk_symbols = (plcpPayloadStart - previous).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                            
                }

               else{
                    
                  chunk_mcs = WifiModeToMcs (headerMode);
				  chunk_symbols = (plcpHtTrainingSymbolsStart - plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                      
                                                      
                                                      
                    
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart - previous).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                   
                                                      
                 }
              }
           //Case 4b: current in HT training symbol. non HT will not come here since it went in previous if or if the previous ifis not true this will be not true        
          else if (current >=plcpHtTrainingSymbolsStart)
             {
                
                  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (plcpHtTrainingSymbolsStart - plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                   
                                                  
                                                  
                
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart - previous).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                        
                                                   
              }
          //Case 4c: current in H sig.non HT will not come here since it went in previous if or if the previous ifis not true this will be not true
          else if (current >=plcpHsigHeaderStart)
             {
				 
				  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (current - plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                   
                                                  
                                                  
                
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart - previous).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp; 

             }
         //Case 4d: Current with prev in L SIG
         else 
            {
                //Case 4di: Non HT format (No HT-SIG or Training Symbols)
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT) //plcpHtTrainingSymbolsStart==plcpHeaderStart)
                {
                    
                  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (current - previous).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                            
                                                
                }
               else
                {
                
                  double chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  double chunk_symbols = (current - previous).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                      
                                                 
                }
            }
        }
      //Case 5: previous is in the preamble works for all cases
      else
        {
          if (current >= plcpPayloadStart)
            {
              //for all
              
                  double chunk_mcs = WifiModeToMcs (payloadMode);
				  double chunk_symbols = (current - plcpPayloadStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  payloadMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                 
                                                
             
               // Non HT format (No HT-SIG or Training Symbols)
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT) {
                 
                  chunk_mcs = WifiModeToMcs (headerMode);
				  chunk_symbols = (plcpPayloadStart - plcpHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                    
                                                    
               }                                 
              else {
              // Greenfield or Mixed format
                
                  chunk_mcs = WifiModeToMcs (headerMode);
				  chunk_symbols = (plcpHtTrainingSymbolsStart - plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                  
			  }
              if (preamble == WIFI_PREAMBLE_HT_MF) {
                 
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart-plcpHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                  
                                                   
			  }            
            }
          else if (current >=plcpHtTrainingSymbolsStart )
          { 
              // Non HT format will not come here since it will execute prev if
              // Greenfield or Mixed format
                
                  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (plcpHtTrainingSymbolsStart - plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                   
                                                  
              if (preamble == WIFI_PREAMBLE_HT_MF)
                  
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart-plcpHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                        
                                                         
           }
          //non HT will not come here     
          else if (current >=plcpHsigHeaderStart)
             { 
                
                  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (current- plcpHsigHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                  
                                                  
                if  (preamble != WIFI_PREAMBLE_HT_GF)
                 {
                     
                  chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  chunk_symbols = (plcpHsigHeaderStart-plcpHeaderStart).GetMicroSeconds() / 4.0;
				  per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                  
                                                       
                  }          
             }
          // GF will not come here
          else if (current >= plcpHeaderStart)
            {
               if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT)
                 {
                 
                  double chunk_mcs = WifiModeToMcs (headerMode);
				  double chunk_symbols = (current - plcpHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  headerMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                  
                                                    
                 }
              else
                 {
              
                                              
                  double chunk_mcs = WifiModeToMcs (MfHeaderMode);
				  double chunk_symbols = (current - plcpHeaderStart).GetMicroSeconds() / 4.0;
				  double per = getPer(coding, PL0, chunk_mcs, CalculateSnr (powerW,
																  noiseInterferenceW,
																  MfHeaderMode));
				  double psr_temp = std::pow(1-per,chunk_symbols/total_symbols);
				  if(psr_temp < psr)
					psr = psr_temp;                                                                
                                               
                   }
            }
        }

      noiseInterferenceW += (*j).GetDelta ();
      previous = (*j).GetTime ();
      j++;
      if(current-starting > MicroSeconds(44))
			break;
    }

  double final_per = 1 - psr;
  return final_per;
}

double
InterferenceHelper::CalculatePer (Ptr<const InterferenceHelper::Event> event, NiChanges *ni) const
{   
  uint64_t activeChannelsPos = (uint64_t)(*(event->GetRxPowerW () - 2));
  uint64_t i;
  uint64_t a = activeChannelsPos % 10;
  uint64_t b = (activeChannelsPos / 10) % 10;
  double powerW = 0.0;
  for(i = a; i <= b; i++) {
	  powerW += (*(event->GetRxPowerW () + i));
  }
	
	
  double psr = 1.0; /* Packet Success Rate */
  NiChanges::iterator j = ni->begin ();
  Time previous = (*j).GetTime ();
  WifiMode payloadMode = event->GetPayloadMode ();
  WifiPreamble preamble = event->GetPreambleType ();
 WifiMode MfHeaderMode ;
 if (preamble==WIFI_PREAMBLE_HT_MF)
   {
    MfHeaderMode = WifiPhy::GetMFPlcpHeaderMode (payloadMode, preamble); //return L-SIG mode

   }
  WifiMode headerMode = WifiPhy::GetPlcpHeaderMode (payloadMode, preamble);
  Time plcpHeaderStart = (*j).GetTime () + MicroSeconds (WifiPhy::GetPlcpPreambleDurationMicroSeconds (payloadMode, preamble)); //packet start time+ preamble
  Time plcpHsigHeaderStart=plcpHeaderStart+ MicroSeconds (WifiPhy::GetPlcpHeaderDurationMicroSeconds (payloadMode, preamble));//packet start time+ preamble+L SIG
  Time plcpHtTrainingSymbolsStart = plcpHsigHeaderStart + MicroSeconds (WifiPhy::GetPlcpHtSigHeaderDurationMicroSeconds (payloadMode, preamble));//packet start time+ preamble+L SIG+HT SIG
  Time plcpPayloadStart =plcpHtTrainingSymbolsStart + MicroSeconds (WifiPhy::GetPlcpHtTrainingSymbolDurationMicroSeconds (payloadMode, preamble,event->GetTxVector())); //packet start time+ preamble+L SIG+HT SIG+Training
  double noiseInterferenceW = (*j).GetDelta ();
  //double powerW = *(event->GetRxPowerW ());
    j++;
  while (ni->end () != j)
    {
      Time current = (*j).GetTime ();
      NS_ASSERT (current >= previous);
      //Case 1: Both prev and curr point to the payload
      if (previous >= plcpPayloadStart)
        {
          psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                          noiseInterferenceW,
                                                          payloadMode),
                                            current - previous,
                                            payloadMode);
           
        }
      //Case 2: previous is before payload
      else if (previous >= plcpHtTrainingSymbolsStart)
        {
          //Case 2a: current is after payload
          if (current >= plcpPayloadStart)
            { 
               //Case 2ai and 2aii: All formats
               psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              payloadMode),
                                                current - plcpPayloadStart,
                                                payloadMode);
                
              }
        }
      //Case 3: previous is in HT-SIG: Non HT will not enter here since it didn't enter in the last two and they are all the same for non HT
      else if (previous >=plcpHsigHeaderStart)
        {
          //Case 3a: cuurent after payload start
          if (current >=plcpPayloadStart)
             {
                   psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              payloadMode),
                                                current - plcpPayloadStart,
                                                payloadMode);
                 
                    psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              headerMode),
                                               plcpHtTrainingSymbolsStart - previous,
                                                headerMode);
              }
          //case 3b: current after HT training symbols start
          else if (current >=plcpHtTrainingSymbolsStart)
             {
                psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                noiseInterferenceW,
                                                                headerMode),
                                                   plcpHtTrainingSymbolsStart - previous,
                                                   headerMode);  
                   
             }
         //Case 3c: current is with previous in HT sig
         else
            {
                psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                noiseInterferenceW,
                                                                headerMode),
                                                   current- previous,
                                                   headerMode);  
                   
            }
      }
      //Case 4: previous in L-SIG: GF will not reach here because it will execute the previous if and exit
      else if (previous >= plcpHeaderStart)
        {
          //Case 4a: current after payload start  
          if (current >=plcpPayloadStart)
             {
                   psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              payloadMode),
                                                      current - plcpPayloadStart,
                                                      payloadMode);
                    //Case 4ai: Non HT format (No HT-SIG or Training Symbols)
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT) //plcpHtTrainingSymbolsStart==plcpHeaderStart)
                {
                    psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              headerMode),
                                                plcpPayloadStart - previous,
                                                headerMode);
                }

               else{
                    psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              headerMode),
                                                      plcpHtTrainingSymbolsStart - plcpHsigHeaderStart,
                                                      headerMode);
                    psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                    noiseInterferenceW,
                                                                    MfHeaderMode),
                                                      plcpHsigHeaderStart - previous,
                                                      MfHeaderMode);
                 }
              }
           //Case 4b: current in HT training symbol. non HT will not come here since it went in previous if or if the previous ifis not true this will be not true        
          else if (current >=plcpHtTrainingSymbolsStart)
             {
                psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              headerMode),
                                                  plcpHtTrainingSymbolsStart - plcpHsigHeaderStart,
                                                  headerMode);
                psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                noiseInterferenceW,
                                                                MfHeaderMode),
                                                   plcpHsigHeaderStart - previous,
                                                   MfHeaderMode);
              }
          //Case 4c: current in H sig.non HT will not come here since it went in previous if or if the previous ifis not true this will be not true
          else if (current >=plcpHsigHeaderStart)
             {
                psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                noiseInterferenceW,
                                                                headerMode),
                                                  current - plcpHsigHeaderStart,
                                                  headerMode);
                 psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                 noiseInterferenceW,
                                                                 MfHeaderMode),
                                                   plcpHsigHeaderStart - previous,
                                                   MfHeaderMode);

             }
         //Case 4d: Current with prev in L SIG
         else 
            {
                //Case 4di: Non HT format (No HT-SIG or Training Symbols)
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT) //plcpHtTrainingSymbolsStart==plcpHeaderStart)
                {
                    psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              headerMode),
                                                current - previous,
                                                headerMode);
                }
               else
                {
                psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                               noiseInterferenceW,
                                                               MfHeaderMode),
                                                 current - previous,
                                                 MfHeaderMode);
                }
            }
        }
      //Case 5: previous is in the preamble works for all cases
      else
        {
          if (current >= plcpPayloadStart)
            {
              //for all
              psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                              payloadMode),
                                                current - plcpPayloadStart,
                                                payloadMode); 
             
               // Non HT format (No HT-SIG or Training Symbols)
              if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT)
                 psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                 noiseInterferenceW,
                                                                  headerMode),
                                                    plcpPayloadStart - plcpHeaderStart,
                                                    headerMode);
              else
              // Greenfield or Mixed format
                psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                noiseInterferenceW,
                                                                headerMode),
                                                  plcpHtTrainingSymbolsStart - plcpHsigHeaderStart,
                                                  headerMode);
              if (preamble == WIFI_PREAMBLE_HT_MF)
                 psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                 noiseInterferenceW,
                                                                 MfHeaderMode),
                                                   plcpHsigHeaderStart-plcpHeaderStart,
                                                   MfHeaderMode);             
            }
          else if (current >=plcpHtTrainingSymbolsStart )
          { 
              // Non HT format will not come here since it will execute prev if
              // Greenfield or Mixed format
                psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                noiseInterferenceW,
                                                                headerMode),
                                                  plcpHtTrainingSymbolsStart - plcpHsigHeaderStart,
                                                  headerMode);
              if (preamble == WIFI_PREAMBLE_HT_MF)
                 psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                 noiseInterferenceW,
                                                                 MfHeaderMode),
                                                   plcpHsigHeaderStart-plcpHeaderStart,
                                                   MfHeaderMode);       
           }
          //non HT will not come here     
          else if (current >=plcpHsigHeaderStart)
             { 
                psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                noiseInterferenceW,
                                                                headerMode),
                                                  current- plcpHsigHeaderStart,
                                                  headerMode); 
                if  (preamble != WIFI_PREAMBLE_HT_GF)
                 {
                   psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                   noiseInterferenceW,
                                                                   MfHeaderMode),
                                                     plcpHsigHeaderStart-plcpHeaderStart,
                                                     MfHeaderMode);    
                  }          
             }
          // GF will not come here
          else if (current >= plcpHeaderStart)
            {
               if (preamble == WIFI_PREAMBLE_LONG || preamble == WIFI_PREAMBLE_SHORT)
                 {
                 psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                                 noiseInterferenceW,
                                                                  headerMode),
                                                    current - plcpHeaderStart,
                                                    headerMode);
                 }
              else
                 {
              psr *= CalculateChunkSuccessRate (CalculateSnr (powerW,
                                                              noiseInterferenceW,
                                                             MfHeaderMode),
                                               current - plcpHeaderStart,
                                               MfHeaderMode);
                       }
            }
        }

      noiseInterferenceW += (*j).GetDelta ();
      previous = (*j).GetTime ();
      j++;
    }

  double per = 1 - psr;
  return per;
}


struct InterferenceHelper::SnrPer
InterferenceHelper::CalculateSnrPer (Ptr<InterferenceHelper::Event> event, bool useLUT, uint32_t coding)
{
  NiChanges ni;
  uint64_t activeChannelsPos = (uint64_t)(*(event->GetRxPowerW () - 2));
  uint64_t i;
  uint64_t a = activeChannelsPos % 10;
  uint64_t b = (activeChannelsPos / 10) % 10;
  //std::cout<<a<<"   "<<b<<std::endl;
  double rxPow = 0.0;
  for(i = a; i <= b; i++) {
	  rxPow += (*(event->GetRxPowerW () + i));
  }
  
  double noiseInterferenceW = CalculateNoiseInterferenceW (event, &ni);
  double snr = CalculateSnr (rxPow,
                             noiseInterferenceW,
                             event->GetPayloadMode ());

  /* calculate the SNIR at the start of the packet and accumulate
   * all SNIR changes in the snir vector.
   */
   
  double per = 0.0;
  if(useLUT)
	  per = CalculatePer_LUT (event, &ni, coding);
  else  
	  per = CalculatePer (event, &ni);

  struct SnrPer snrPer;
  snrPer.snr = snr;
  snrPer.per = per;
  return snrPer;
}

void
InterferenceHelper::EraseEvents (void)
{
  uint64_t i;
  for(i = 0; i < m_niChangesList.size(); i++) {	
	  (m_niChangesList.at(i)).clear ();
	  m_rxing = false;
	  m_firstPowers[i] = 0.0;
  }
}
InterferenceHelper::NiChanges::iterator
InterferenceHelper::GetPosition (Time moment)
{
  return std::upper_bound (m_niChanges.begin (), m_niChanges.end (), NiChange (moment, 0));

}
void
InterferenceHelper::AddNiChangeEvent (NiChange change)
{
  m_niChanges.insert (GetPosition (change.GetTime ()), change);
}
void
InterferenceHelper::NotifyRxStart ()
{
  m_rxing = true;
}
void
InterferenceHelper::NotifyRxEnd ()
{
  m_rxing = false;
}
} // namespace ns3
