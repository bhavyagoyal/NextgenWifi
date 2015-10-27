/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/propagation-module.h"

//This is a simple example in order to test the new interference model
//
//Network topology:
// 
//  Wifi 192.168.1.0		
//
//			   n2				n3
//			   |  				| 							
//            AP1 - n1         AP2
//        						|
//							    n4
//


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleWifiSimulation");

double throughput1=0;
double throughput2=0;
double throughput3=0;
double throughput4=0;
double throughput5=0;
double throughput6=0;



//===========================================================================
//Set position of the nodes
//===========================================================================
static void
SetPosition (Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  mobility->SetPosition (position);
}

int main (int argc, char *argv[])
{
  uint32_t logging = 0;
  uint32_t newModel = 1;
  uint32_t checkAddr = 0;
  CommandLine cmd;
 /* 
  * Enable debug mode by making logging = 1 from command line while running the script.
  * Enabling debugging mode will print a lot of parameters like SNR, PER, transmitted power, 
  * received power, the state of phy, etc. for each packet.
  */
  cmd.AddValue ("logging", "Log debug variables or not (0 or 1)", logging);
  
 /*
  * Setting newModel = 1 using command line will make the simulation
  * use the new interference model, if set to 0, then original ns3 model will be used.
  */
  cmd.AddValue ("newModel", "Use new interference model or not (0 or 1)", newModel);
 /*
  * Setting checkAddrAtPhy = 1 using command line will make the simulation
  * use the new feature of examining the MAC header at phy layer, 
  * to see if the packet is meant for that STA or not,
  * if set to 0, then original ns3 logic will be used.
  */
  cmd.AddValue ("checkAddrAtPhy", "Use the feature of inspecting MAC header for RA at phy layer or not (0 or 1)", checkAddr);
  cmd.Parse (argc,argv);
  
  if (logging) {
	LogComponentEnable ("YansWifiPhy", LOG_LEVEL_DEBUG);
  }
  
  // 1. Create 4 nodes and 2 APs
  NodeContainer wifiNodes1;
  wifiNodes1.Create (2); //the parameter passed to Create is the number of nodes to be created, so we are creating 2 nodes here.
  NodeContainer wifiApNode1;
  wifiApNode1.Create (1);
  NodeContainer wifiNodes2;
  wifiNodes2.Create (2);
  NodeContainer wifiApNode2;
  wifiApNode2.Create (1);
  
  // 2. Create channel B propagation loss model
  Ptr<ChannelBPropagationLossModel> lossModel = CreateObject<ChannelBPropagationLossModel> ();
  //Ptr<OutdoorPropagationLossModel> lossModel = CreateObject<OutdoorPropagationLossModel> ();
  lossModel->SetFrequency(2.4e9); //frequency is in Hz

  // 3. Create & set-up wifi channel
  Ptr<YansWifiChannel> wifiChannel = CreateObject <YansWifiChannel> ();
  wifiChannel->SetPropagationLossModel (lossModel);
  //this model sets the propagation speed of signal to be constant (equal to the speed of light).
  wifiChannel->SetPropagationDelayModel (CreateObject <ConstantSpeedPropagationDelayModel> ());
 
 // 4. Create & set-up phy layer
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetErrorRateModel ("ns3::YansErrorRateModel");
  phy.Set ("EnergyDetectionThreshold", DoubleValue(-72.0)); //in dBm
  phy.Set ("CcaMode1Threshold", DoubleValue(-82.0)); //in dBm
  //phy.Set ("ShortGuardEnabled",BooleanValue(true));
 /*
  * ns3 allows a fixed number of transmission power levels
  * between TxPowerStart and TxPowerEnd, but we keep both
  * of them to be same to have a single value of TxPower
  */
  phy.Set ("TxPowerStart", DoubleValue(20.0)); //in dBm
  phy.Set ("TxPowerEnd", DoubleValue(20.0)); //in dBm
  phy.Set ("Frequency", UintegerValue(2400)); // In MHz
  phy.Set("BW", UintegerValue(40)); // in MHz
  phy.Set ("ChannelNumber", UintegerValue (203));
  phy.Set ("PrimaryChannel", UintegerValue (2));
  phy.SetChannel (wifiChannel); //associate the wifiChannel with the phy
  
 /* Setting the phy layer on whether to 
  * use old or new interference model, and whether to use 
  * the feature of MAC header examination at phy layer or not.
  * If new model is not used, then MAC header examination is also
  * switched off.
  */
  if(newModel) {
	phy.Set("UseNewModel", BooleanValue (true));
	if (checkAddr) {
		phy.Set("CheckAddrAtPhy", BooleanValue (true));
	}
	else {
		phy.Set("CheckAddrAtPhy", BooleanValue (false));
	}
  }
  else {
	phy.Set("UseNewModel", BooleanValue (false));
	phy.Set("CheckAddrAtPhy", BooleanValue (false));
  }

 // 5. Install wireless devices
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

 /*
  * HtWifiMac is necessary while using HT rates,
  * when using OFDM rates, use QosWifiMac.
  * By default QoS is supported in HtWifiMac.
  */  
  HtWifiMacHelper mac = HtWifiMacHelper::Default ();
  //QosWifiMacHelper mac = QosWifiMacHelper::Default ();
  
  StringValue DataRate;
 /*
  * For a complete of list of supported data rates - 
  *  http://www.nsnam.org/doxygen-release/wifi-phy_8cc_source.html#l00504
  */
  //DataRate = StringValue("OfdmRate52MbpsBW20MHz"); //HT rate, 52Mbps, BW 20MHz
  std::string str = "OfdmRate108MbpsBW40MHz";
  DataRate = StringValue(str);
 /*
  * Here we select our rate control algorithm to be the constant rate algorithm.
  * The data rate which we mention for data mode is the one used for transmitting data packets.
  * The data rate which we mention for the control mode is used only for the RTS packets.
  * The rate at which control answer packets are sent (i.e. ACK or CTS), is decided based
  * on the data rate of data mode according to rules given here-
  * http://www.nsnam.org/doxygen-release/classns3_1_1_wifi_remote_station_manager.html#aa39d276e0171076b7ebc98b5cb8b0947
  * Finally the Beacons and ARP requests are sent at a fixed rate of 1Mbps.
  * By default, both DataMode rate and ControlMode rate are set to 6Mbps,
  * so if we do net set either of them, 6Mbps will be used.
  */
  
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", DataRate,
                                   "ControlMode", DataRate);
  
  // Here we define two different SSIDs for the two BSS.
  Ssid ssid1 = Ssid ("ns-3-802.11n1");
  Ssid ssid2 = Ssid ("ns-3-802.11n2");
  
  
  // BSS 1
  
 /*
  * We set the MAC type to that of non-AP station (STA) 
  * in an infrastructure BSS (i.e., a BSS with an AP). 
  * We set the SSID to that of BSS1.
  * Finally, the “ActiveProbing” Attribute is set to false. 
  * This means that probe requests will not be sent by the MAC.
  */
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (false));
  //mac.SetBlockAckThresholdForAc (AC_BE, 2);
  //mac.SetMpduAggregatorForAc ("ns3::MpduStandardAggregator", 
  //                         "MaxAmpduSize", UintegerValue (5*1024));
  //phy.Set ("NodeType", UintegerValue(1));
  NetDeviceContainer staDevices1;
  staDevices1 = wifi.Install (phy, mac, wifiNodes1); //We now install the phy and mac on stations of BSS1
  
 /*
  * We set the MAC type to that of AP station.
  * We set the SSID to that of BSS1.
  * As the SSID of stations and AP becomes same,
  * they will associate with each other.
  */
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1));
  //phy.Set ("NodeType", UintegerValue(1));             
  NetDeviceContainer apDevice1;
  apDevice1 = wifi.Install (phy, mac, wifiApNode1); //We now install the phy and mac on AP of BSS1
  
  // BSS 2
  
 /* Similar procedures for BSS2, except that we
  * change the data rate, BW and the channel number.
  */
  DataRate = StringValue("OfdmRate108MbpsBW40MHz");  
  //DataRate = StringValue("OfdmRate52MbpsBW20MHz"); //HT rate, 52Mbps, BW 20MHz
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", DataRate,
                                   "ControlMode", DataRate);
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid2),
               "ActiveProbing", BooleanValue (false));
  //phy.Set ("NodeType", UintegerValue(1));

  phy.Set("BW", UintegerValue(40));
  phy.Set ("ChannelNumber", UintegerValue (607)); 
  phy.Set ("PrimaryChannel", UintegerValue (6));
  phy.Set ("TxPowerStart", DoubleValue(20.0)); //in dBm
  phy.Set ("TxPowerEnd", DoubleValue(20.0)); //in dBm
  NetDeviceContainer staDevices2;
  staDevices2 = wifi.Install (phy, mac, wifiNodes2);  
  
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2));
  //phy.Set ("NodeType", UintegerValue(1));
  NetDeviceContainer apDevice2;
  apDevice2 = wifi.Install (phy, mac, wifiApNode2);
  
   // 6. Place stations and APs
   MobilityHelper mobility;
   //We define the constant position model and install it on all nodes.
   mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
   mobility.Install (wifiNodes1);
   mobility.Install (wifiApNode1);
   mobility.Install (wifiNodes2);
   mobility.Install (wifiApNode2);
    
   // Here we set the (x,y,z) co-ordinates of all the nodes(in metres)
   SetPosition (wifiNodes1.Get(0), Vector (20.0,0.0,0.0));  
   SetPosition (wifiNodes1.Get(1), Vector (0.0,20.0,0.0));
   SetPosition (wifiNodes2.Get(0), Vector (40.0,5.0,0.0));
   SetPosition (wifiNodes2.Get(1), Vector (40.0,-5.0,0.0));
   SetPosition (wifiApNode1.Get(0), Vector (0.0,0.0,0.0));
   SetPosition (wifiApNode2.Get(0), Vector (40.0,0.0,0.0));

  

  // 7. Install TCP/IP stack & assign IP addresses
  InternetStackHelper stack;
  stack.Install (wifiApNode1);
  stack.Install (wifiNodes1);
  stack.Install (wifiApNode2);
  stack.Install (wifiNodes2);

  Ipv4AddressHelper address1;
 /* We define an Ipv4 address Helper.
  * The first parameter is the base address and
  * the second address is the mask.
  * So the addresses will start from 192.168.1.1
  * and can go up-to 192.168.1.254
  * If more addresses are required then change the mask
  */
  address1.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiNodesInterfaces1;
  Ipv4InterfaceContainer apNodeInterface1;
  Ipv4InterfaceContainer wifiNodesInterfaces2;
  Ipv4InterfaceContainer apNodeInterface2;

 /* The Assign function will allocate the addresses sequentially.
  * So the two STAs of BSS1 will get 192.168.1.1 and 192.168.1.2
  * The AP of BSS1 will get 192.168.1.3, and so on.
  */
  wifiNodesInterfaces1 = address1.Assign (staDevices1);
  apNodeInterface1 = address1.Assign (apDevice1);
  
  wifiNodesInterfaces2 = address1.Assign (staDevices2);
  apNodeInterface2 = address1.Assign (apDevice2);
  
  
  // 8. Applications
  
  uint64_t rate = 100000000; //in bps
  double t=10; // the stop time for applications (in seconds)

//UDP flow 1 ---- ap1 to n1
 //We first define a sink for UDP packets at n1
       ApplicationContainer serverApps1,sink1App1;
       uint16_t port1 = 4000;
       Address apLocalAddress1 (InetSocketAddress (Ipv4Address("192.168.1.1"), port1));
       PacketSinkHelper packetSinkHelper1 ("ns3::UdpSocketFactory", apLocalAddress1);
       sink1App1 = packetSinkHelper1.Install (wifiNodes1.Get (0)); // Install  the sink on n1

       sink1App1.Start (Seconds (0.0)); // Start time for application (in seconds)
	  /*
	   * Stop time for application (in seconds).
	   * If no stop time is assigned, then the application will
	   * go on for ever, unless a simulation stop time is enforced.
	   */
       sink1App1.Stop (Seconds (t));
 
/* Next we define an on-off application and install it on ap1
 * This application generates packets of size "PacketSize",
 * at a rate of "DataRate", for the duration of "OnTime",
 * then it remains silent for "OffTime", then again starts
 * for "OnTime" and so on.
 * Here we want continuous traffic, so we set "OffTime" to be 0.
 */
       OnOffHelper onoff1("ns3::UdpSocketFactory",Ipv4Address("192.168.1.1"));
       onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=60]"));//in seconds
       onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
       onoff1.SetAttribute ("PacketSize", UintegerValue (1000)); //in bytes
       onoff1.SetAttribute ("DataRate", DataRateValue (rate));
       ApplicationContainer apps1;

       AddressValue remoteAddress1 (InetSocketAddress (wifiNodesInterfaces1.GetAddress(0), port1)); //address of n1
       onoff1.SetAttribute ("Remote", remoteAddress1);// tell the on-off application to send packets to n1
       apps1.Add (onoff1.Install (wifiApNode1.Get (0)));// install application on AP1
       apps1.Start (Seconds (0.0));
       apps1.Stop (Seconds (t));

//UDP flow 2 ---- ap1 to n2
       ApplicationContainer serverApps2,sink1App2;
       uint16_t port2 = 5000;
       Address apLocalAddress2 (InetSocketAddress (Ipv4Address("192.168.1.2"), port2));
       PacketSinkHelper packetSinkHelper2 ("ns3::UdpSocketFactory", apLocalAddress2);
       sink1App2 = packetSinkHelper2.Install (wifiNodes1.Get (1));

       sink1App2.Start (Seconds (0.001));
       sink1App2.Stop (Seconds (t));
         
       OnOffHelper onoff2("ns3::UdpSocketFactory",Ipv4Address("192.168.1.2"));
       onoff2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=60]"));//in seconds
       onoff2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
       onoff2.SetAttribute ("PacketSize", UintegerValue (1000));
       onoff2.SetAttribute ("DataRate", DataRateValue (rate));
       ApplicationContainer apps2;

       AddressValue remoteAddress2 (InetSocketAddress (wifiNodesInterfaces1.GetAddress(1), port2));
       onoff2.SetAttribute ("Remote", remoteAddress2);
       apps2.Add (onoff2.Install (wifiApNode1.Get (0)));
       apps2.Start (Seconds (0.001));
       apps2.Stop (Seconds (t));
	    
//UDP flow 3 ----- n1 to ap1
       ApplicationContainer serverApps3,sink1App3;
       uint16_t port3 = 6000;
       Address apLocalAddress3 (InetSocketAddress (Ipv4Address ("192.168.1.3"), port3));
       PacketSinkHelper packetSinkHelper3 ("ns3::UdpSocketFactory", apLocalAddress3);
       sink1App3 = packetSinkHelper3.Install (wifiApNode1.Get (0));

       sink1App3.Start (Seconds (0.002));
       sink1App3.Stop (Seconds (t));
            
       OnOffHelper onoff3 ("ns3::UdpSocketFactory",Ipv4Address ("192.168.1.3"));
       onoff3.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=60]"));//in seconds
       onoff3.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
       onoff3.SetAttribute ("PacketSize", UintegerValue (1000));
       onoff3.SetAttribute ("DataRate", DataRateValue (rate));
       ApplicationContainer apps3;

       AddressValue remoteAddress3 (InetSocketAddress (apNodeInterface1.GetAddress(0), port3));
       onoff3.SetAttribute ("Remote", remoteAddress3);
       apps3.Add (onoff3.Install (wifiNodes1.Get (0)));
       apps3.Start (Seconds (0.002));
       apps3.Stop (Seconds (t));
	   	   
	   
//UDP flow 4 ---- ap2 to n3
       ApplicationContainer serverApps4,sink1App4;
       uint16_t port4 = 7000;
       Address apLocalAddress4 (InetSocketAddress (Ipv4Address("192.168.1.4"), port4));
       PacketSinkHelper packetSinkHelper4 ("ns3::UdpSocketFactory", apLocalAddress4);
       sink1App4 = packetSinkHelper4.Install (wifiNodes2.Get (0));

       sink1App4.Start (Seconds (0.003));
       sink1App4.Stop (Seconds (t));
         
       OnOffHelper onoff4("ns3::UdpSocketFactory",Ipv4Address("192.168.1.4"));
       onoff4.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=60]"));//in seconds
       onoff4.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
       onoff4.SetAttribute ("PacketSize", UintegerValue (1000));
       onoff4.SetAttribute ("DataRate", DataRateValue (rate));
       ApplicationContainer apps4;

       AddressValue remoteAddress4 (InetSocketAddress (wifiNodesInterfaces2.GetAddress(0), port4));
       onoff4.SetAttribute ("Remote", remoteAddress4);
       apps4.Add (onoff4.Install (wifiApNode2.Get (0)));
       apps4.Start (Seconds (0.003));
       apps4.Stop (Seconds (t));

//UDP flow 5 ---- ap2 to n4
       ApplicationContainer serverApps5,sink1App5;
       uint16_t port5 = 8000;
       Address apLocalAddress5 (InetSocketAddress (Ipv4Address("192.168.1.5"), port5));
       PacketSinkHelper packetSinkHelper5 ("ns3::UdpSocketFactory", apLocalAddress5);
       sink1App5 = packetSinkHelper5.Install (wifiNodes2.Get (1));

       sink1App5.Start (Seconds (0.004));
       sink1App5.Stop (Seconds (t));
         
       OnOffHelper onoff5("ns3::UdpSocketFactory",Ipv4Address("192.168.1.5"));
       onoff5.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=60]"));//in seconds
       onoff5.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
       onoff5.SetAttribute ("PacketSize", UintegerValue (1000));
       onoff5.SetAttribute ("DataRate", DataRateValue (rate));
       ApplicationContainer apps5;

       AddressValue remoteAddress5 (InetSocketAddress (wifiNodesInterfaces2.GetAddress(1), port5));
       onoff5.SetAttribute ("Remote", remoteAddress5);
       apps5.Add (onoff5.Install (wifiApNode2.Get (0)));
       apps5.Start (Seconds (0.004));
       apps5.Stop (Seconds (t));
	    
//UDP flow 6 ----- n3 to ap2
       ApplicationContainer serverApps6,sink1App6;
       uint16_t port6 = 9000;
       Address apLocalAddress6 (InetSocketAddress (Ipv4Address ("192.168.1.6"), port6));
       PacketSinkHelper packetSinkHelper6 ("ns3::UdpSocketFactory", apLocalAddress6);
       sink1App6 = packetSinkHelper6.Install (wifiApNode2.Get (0));

       sink1App6.Start (Seconds (0.005));
       sink1App6.Stop (Seconds (t));
            
       OnOffHelper onoff6 ("ns3::UdpSocketFactory",Ipv4Address ("192.168.1.6"));
       onoff6.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=60]"));//in seconds
       onoff6.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
       onoff6.SetAttribute ("PacketSize", UintegerValue (1000));
       onoff6.SetAttribute ("DataRate", DataRateValue (rate));
       ApplicationContainer apps6;

       AddressValue remoteAddress6 (InetSocketAddress (apNodeInterface2.GetAddress(0), port6));
       onoff6.SetAttribute ("Remote", remoteAddress6);
       apps6.Add (onoff6.Install (wifiNodes2.Get (0)));
       apps6.Start (Seconds (0.005));
       apps6.Stop (Seconds (t));
  
  
  //Config::Connect ("/NodeList/*/ApplicationList/*/Tx", MakeBoundCallback (&TagMarker, AC_VO));
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables (); //populate routing tables

 /* Simulation we just created will never “naturally” stop. This
  * is because the wireless access point will generate beacons. 
  * It will generate beacons forever, and this will result
  * in simulator events being scheduled into the future indefinitely,
  * so we must tell the simulator to stop even though it
  * may have beacon generation events scheduled. 
  * The following line of code tells the simulator to stop so that we don’t
  * simulate beacons forever and enter what is essentially an endless loop.
  */
  Simulator::Stop (Seconds (10.0));  //in seconds
  
  
 /*
  * For a broad overview of the simulation events,
  * we can enable Pcap tracing at any node,
  * and later analyse the traffic by opening the
  * generated trace file in tcpdump or Wireshark
  */
 // phy.EnablePcap ("basic_testing_BSS_1", 
 //                 wifiApNode1.Get (0)->GetId (), 0);
 // phy.EnablePcap ("basic_testing_BSS_2", 
 //                wifiApNode2.Get (0)->GetId (), 0);			  
				  
  Simulator::Run ();
  Simulator::Destroy ();
  
  
  uint32_t totalBytesReceived1 = DynamicCast<PacketSink>(sink1App1.Get (0))->GetTotalRx (); // get the total bytes received by this sink application
  throughput1=totalBytesReceived1*8/(t*1000000.0); // calculate throughput in Mbps
  
  uint32_t totalBytesReceived2 = DynamicCast<PacketSink>(sink1App2.Get (0))->GetTotalRx ();
  throughput2=totalBytesReceived2*8/(t*1000000.0);
  
  uint32_t totalBytesReceived3 = DynamicCast<PacketSink>(sink1App3.Get (0))->GetTotalRx ();
  throughput3=totalBytesReceived3*8/(t*1000000.0);
  
  uint32_t totalBytesReceived4 = DynamicCast<PacketSink>(sink1App4.Get (0))->GetTotalRx ();
  throughput4=totalBytesReceived4*8/(t*1000000.0);
  
  uint32_t totalBytesReceived5 = DynamicCast<PacketSink>(sink1App5.Get (0))->GetTotalRx ();
  throughput5=totalBytesReceived5*8/(t*1000000.0);
  
  uint32_t totalBytesReceived6 = DynamicCast<PacketSink>(sink1App6.Get (0))->GetTotalRx ();
  throughput6=totalBytesReceived6*8/(t*1000000.0);
  
  
  std::cout<<"throughput at n1 = "<<throughput1<<std::endl;
  std::cout<<"throughput at n2 = "<<throughput2<<std::endl;
  std::cout<<"throughput at AP1 = "<<throughput3<<std::endl;
  std::cout<<"throughput at n3 = "<<throughput4<<std::endl;
  std::cout<<"throughput at n4 = "<<throughput5<<std::endl;
  std::cout<<"throughput at AP2 = "<<throughput6<<std::endl;
  std::cout<<"Aggregate throughput at BSS1 = "<<(throughput1+throughput2+throughput3)<<std::endl;
  std::cout<<"Aggregate throughput at BSS2 = "<<(throughput4+throughput5+throughput6)<<std::endl;
  std::cout<<"Net throughput = "<<(throughput1+throughput2+throughput3+throughput4+throughput5+throughput6)/40.0<<" Mbps/MHz"<<std::endl;


  return 0;
}
