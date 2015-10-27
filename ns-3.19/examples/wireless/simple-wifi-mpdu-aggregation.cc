/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"

//This is a simple example in order to show how 802.11n frame aggregation feature (A-MPDU) works.
//
//
//Packets in this simulation aren't marked with a QosTag so they are considered
//belonging to BestEffort Access Class (AC_BE).

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleWifiFrameAggregation");

double rxBytessum=0;
double throughput=0;


//===========================================================================
//Calculate the throughpur at the AP
//===========================================================================
void
PhyRxOkTrace (std::string context, Ptr<const Packet> packet, double snr, WifiMode mode, enum WifiPreamble preamble)
{
  Ptr<Packet> p1=packet->Copy();
  WifiMacHeader hdr;
  p1->RemoveHeader(hdr);
  //since we only have one rx this will work if we have multiple rx then we need to make sure that this is the intended reciever
  if (hdr.IsData() || hdr.IsQosData())
    {
      rxBytessum+=packet->GetSize();
    }
}

//===========================================================================
//Set position of the nodes
//===========================================================================
static void
SetPosition (Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  mobility->SetPosition (position);
}

//==========================================================================
//==========================================================================

int main (int argc, char *argv[])
{
  std::cout << "AggregationSize" <<"  " << "Throughput" << " " << "Utilization" << '\n';
  bool udp = true;
  int ampduSize = 2;
  for (;ampduSize <= 16; ampduSize+= 2)
{
  uint32_t nWifi = 1;
  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.Parse (argc,argv);
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("990000"));
  // disable rts cts all the time.
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("1500"));
  NodeContainer wifiNodes;
  wifiNodes.Create (3);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);
 
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  //phy.Set ("ShortGuardEnabled",BooleanValue(true));
  //phy.Set ("GreenfieldEnabled",BooleanValue(true));

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);
  HtWifiMacHelper mac = HtWifiMacHelper::Default ();
 

  Ssid ssid = Ssid ("ns380211n");
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue("OfdmRate65MbpsBW20MHz"),
                                   "ControlMode", StringValue ("OfdmRate65MbpsBW20MHz"));
  //wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  mac.SetBlockAckThresholdForAc (AC_BE, 2);

 mac.SetMpduAggregatorForAc ("ns3::MpduStandardAggregator", 
                             "MaxAmpduSize", UintegerValue (ampduSize*1584));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  mac.SetBlockAckThresholdForAc (AC_BE, 2);

  mac.SetMpduAggregatorForAc ("ns3::MpduStandardAggregator", 
                             "MaxAmpduSize", UintegerValue (ampduSize*1584));
 
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);
               
 // mobility.
   MobilityHelper mobility;
   mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
   mobility.Install (wifiNodes);
   mobility.Install (wifiApNode);
    
   SetPosition (wifiNodes.Get(0), Vector (1.0,0.0,0.0));
   SetPosition (wifiNodes.Get(1), Vector (0.0,1.0,0.0));
   SetPosition (wifiNodes.Get(2), Vector (3.0,0.0,0.0));
   SetPosition (wifiApNode.Get(0), Vector (0.0,0.0,0.0));
 

  /* Internet stack*/
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiNodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiNodesInterfaces;
  Ipv4InterfaceContainer apNodeInterface;

  wifiNodesInterfaces = address.Assign (staDevices);
  apNodeInterface = address.Assign (apDevice);

  ApplicationContainer serverApps,sink1App;

  double t=5;

  /* Setting applications */
  if (udp == true)
    {
      UdpServerHelper myServer (9);
      serverApps = myServer.Install (wifiApNode.Get (0));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (t));

      UdpClientHelper myClient (apNodeInterface.GetAddress (0), 9);
      myClient.SetAttribute ("MaxPackets", UintegerValue (64707202));
      myClient.SetAttribute ("Interval", TimeValue (Time ("0.00002")));
      myClient.SetAttribute ("PacketSize", UintegerValue (1500));

      ApplicationContainer clientApps = myClient.Install (wifiNodes);
      clientApps.Start (Seconds (0.0));
      clientApps.Stop (Seconds (t));
    }
  else
    {

      //TCP flow
      uint16_t port = 50000;
      Address apLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
      PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", apLocalAddress);
      sink1App = packetSinkHelper.Install (wifiApNode.Get (0));

      sink1App.Start (Seconds (0.0));
      sink1App.Stop (Seconds (t));
           
      OnOffHelper onoff ("ns3::TcpSocketFactory",Ipv4Address::GetAny ());
      onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=30.0]"));//in seconds
      onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
      onoff.SetAttribute ("PacketSize", UintegerValue (1500));
      onoff.SetAttribute ("DataRate", DataRateValue (100000000));
      ApplicationContainer apps;

      AddressValue remoteAddress (InetSocketAddress (wifiNodesInterfaces.GetAddress(0), port));
      onoff.SetAttribute ("Remote", remoteAddress);
      apps.Add (onoff.Install (wifiNodes));
      apps.Start (Seconds (0.0));
      apps.Stop (Seconds (t));
    }
  //populate routing table
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (t));

 
  Simulator::Run ();

  Simulator::Destroy ();
  //UDP
  if (udp == true)
    {
      uint32_t totalPacketsThrough = DynamicCast<UdpServer>(serverApps.Get (0))->GetReceived ();
      throughput=totalPacketsThrough*1500*8/(t*1000000.0);
    }
  else
    {
      //TCP
      uint32_t totalPacketsThrough = DynamicCast<PacketSink>(sink1App.Get (0))->GetTotalRx ();
      throughput=totalPacketsThrough*8/((t-3)*1000000.0);
    }
  
  std::cout << ampduSize <<"  " << throughput << "  " << throughput/65<< '\n';
  }
  return 0;
}
