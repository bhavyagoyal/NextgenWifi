/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006,2007 INRIA
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
 * Contributions: Timo Bingmann <timo.bingmann@student.kit.edu>
 * Contributions: Tom Hewer <tomhewer@mac.com> for Two Ray Ground Model
 *                Pavel Boyko <boyko@iitp.ru> for matrix
 */

#include "propagation-loss-model.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include <cmath>

NS_LOG_COMPONENT_DEFINE ("PropagationLossModel");

namespace ns3 {

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (PropagationLossModel)
  ;

TypeId 
PropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PropagationLossModel")
    .SetParent<Object> ()
  ;
  return tid;
}

PropagationLossModel::PropagationLossModel ()
  : m_next (0)
{
	m_random = CreateObject<UniformRandomVariable> ();
}

PropagationLossModel::~PropagationLossModel ()
{
}

void
PropagationLossModel::SetNext (Ptr<PropagationLossModel> next)
{
  m_next = next;
}

Ptr<PropagationLossModel>
PropagationLossModel::GetNext ()
{
  return m_next;
}

double
PropagationLossModel::CalcRxPower (double txPowerDbm,
                                   Ptr<MobilityModel> a,
                                   Ptr<MobilityModel> b) const
{
  double self = DoCalcRxPower (txPowerDbm, a, b);
  if (m_next != 0)
    {
      self = m_next->CalcRxPower (self, a, b);
    }
  return self;
}

/*
double
PropagationLossModel::CalcRxPower_STA_STA (double txPowerDbm,
                                   Ptr<MobilityModel> a,
                                   Ptr<MobilityModel> b)
{
  OutdoorPropagationLossModel *model = dynamic_cast<OutdoorPropagationLossModel*>(this);
  double self = model->DoCalcRxPower_STA_STA (txPowerDbm, a, b);
  
  return self;
}

double
PropagationLossModel::CalcRxPower_STA_AP (double txPowerDbm,
                                   Ptr<MobilityModel> a,
                                   Ptr<MobilityModel> b)
{
  OutdoorPropagationLossModel *model = dynamic_cast<OutdoorPropagationLossModel*>(this);
  double self = model->DoCalcRxPower_STA_AP (txPowerDbm, a, b);
  
  return self;
}

double
PropagationLossModel::CalcRxPower_AP_AP (double txPowerDbm,
                                   Ptr<MobilityModel> a,
                                   Ptr<MobilityModel> b)
{
  OutdoorPropagationLossModel *model = dynamic_cast<OutdoorPropagationLossModel*>(this);
  double self = model->DoCalcRxPower_AP_AP (txPowerDbm, a, b);
  
  return self;
}
*/
int64_t
PropagationLossModel::AssignStreams (int64_t stream)
{
  int64_t currentStream = stream;
  currentStream += DoAssignStreams (stream);
  if (m_next != 0)
    {
      currentStream += m_next->AssignStreams (currentStream);
    }
  return (currentStream - stream);
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (RandomPropagationLossModel)
  ;

TypeId 
RandomPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RandomPropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<RandomPropagationLossModel> ()
    .AddAttribute ("Variable", "The random variable used to pick a loss everytime CalcRxPower is invoked.",
                   StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                   MakePointerAccessor (&RandomPropagationLossModel::m_variable),
                   MakePointerChecker<RandomVariableStream> ())
  ;
  return tid;
}
RandomPropagationLossModel::RandomPropagationLossModel ()
  : PropagationLossModel ()
{
}

RandomPropagationLossModel::~RandomPropagationLossModel ()
{
}

double
RandomPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                           Ptr<MobilityModel> a,
                                           Ptr<MobilityModel> b) const
{
  double rxc = -m_variable->GetValue ();
  NS_LOG_DEBUG ("attenuation coefficent="<<rxc<<"Db");
  return txPowerDbm + rxc;
}

int64_t
RandomPropagationLossModel::DoAssignStreams (int64_t stream)
{
  m_variable->SetStream (stream);
  return 1;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (FriisPropagationLossModel)
  ;

const double FriisPropagationLossModel::PI = 3.14159265358979323846;

TypeId 
FriisPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FriisPropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<FriisPropagationLossModel> ()
    .AddAttribute ("Frequency", 
                   "The carrier frequency (in Hz) at which propagation occurs  (default is 5.15 GHz).",
                   DoubleValue (5.150e9),
                   MakeDoubleAccessor (&FriisPropagationLossModel::SetFrequency,
                                       &FriisPropagationLossModel::GetFrequency),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SystemLoss", "The system loss",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&FriisPropagationLossModel::m_systemLoss),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MinDistance", 
                   "The distance under which the propagation model refuses to give results (m)",
                   DoubleValue (0.5),
                   MakeDoubleAccessor (&FriisPropagationLossModel::SetMinDistance,
                                       &FriisPropagationLossModel::GetMinDistance),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}

FriisPropagationLossModel::FriisPropagationLossModel ()
{
}
void
FriisPropagationLossModel::SetSystemLoss (double systemLoss)
{
  m_systemLoss = systemLoss;
}
double
FriisPropagationLossModel::GetSystemLoss (void) const
{
  return m_systemLoss;
}
void
FriisPropagationLossModel::SetMinDistance (double minDistance)
{
  m_minDistance = minDistance;
}
double
FriisPropagationLossModel::GetMinDistance (void) const
{
  return m_minDistance;
}

void
FriisPropagationLossModel::SetFrequency (double frequency)
{
  m_frequency = frequency;
  static const double C = 299792458.0; // speed of light in vacuum
  m_lambda = C / frequency;
}

double
FriisPropagationLossModel::GetFrequency (void) const
{
  return m_frequency;
}

double
FriisPropagationLossModel::DbmToW (double dbm) const
{
  double mw = std::pow (10.0,dbm/10.0);
  return mw / 1000.0;
}

double
FriisPropagationLossModel::DbmFromW (double w) const
{
  double dbm = std::log10 (w * 1000.0) * 10.0;
  return dbm;
}

double 
FriisPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                          Ptr<MobilityModel> a,
                                          Ptr<MobilityModel> b) const
{
  /*
   * Friis free space equation:
   * where Pt, Gr, Gr and P are in Watt units
   * L is in meter units.
   *
   *    P     Gt * Gr * (lambda^2)
   *   --- = ---------------------
   *    Pt     (4 * pi * d)^2 * L
   *
   * Gt: tx gain (unit-less)
   * Gr: rx gain (unit-less)
   * Pt: tx power (W)
   * d: distance (m)
   * L: system loss
   * lambda: wavelength (m)
   *
   * Here, we ignore tx and rx gain and the input and output values 
   * are in dB or dBm:
   *
   *                           lambda^2
   * rx = tx +  10 log10 (-------------------)
   *                       (4 * pi * d)^2 * L
   *
   * rx: rx power (dB)
   * tx: tx power (dB)
   * d: distance (m)
   * L: system loss (unit-less)
   * lambda: wavelength (m)
   */
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_minDistance)
    {
      return txPowerDbm;
    }
  double numerator = m_lambda * m_lambda;
  double denominator = 16 * PI * PI * distance * distance * m_systemLoss;
  double pr = 10 * std::log10 (numerator / denominator);
  NS_LOG_DEBUG ("distance="<<distance<<"m, attenuation coefficient="<<pr<<"dB");
  return txPowerDbm + pr;
}

int64_t
FriisPropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //
// -- Two-Ray Ground Model ported from NS-2 -- tomhewer@mac.com -- Nov09 //

NS_OBJECT_ENSURE_REGISTERED (TwoRayGroundPropagationLossModel)
  ;

const double TwoRayGroundPropagationLossModel::PI = 3.14159265358979323846;

TypeId 
TwoRayGroundPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TwoRayGroundPropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<TwoRayGroundPropagationLossModel> ()
    .AddAttribute ("Frequency", 
                   "The carrier frequency (in Hz) at which propagation occurs  (default is 5.15 GHz).",
                   DoubleValue (5.150e9),
                   MakeDoubleAccessor (&TwoRayGroundPropagationLossModel::SetFrequency,
                                       &TwoRayGroundPropagationLossModel::GetFrequency),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SystemLoss", "The system loss",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&TwoRayGroundPropagationLossModel::m_systemLoss),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MinDistance",
                   "The distance under which the propagation model refuses to give results (m)",
                   DoubleValue (0.5),
                   MakeDoubleAccessor (&TwoRayGroundPropagationLossModel::SetMinDistance,
                                       &TwoRayGroundPropagationLossModel::GetMinDistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("HeightAboveZ",
                   "The height of the antenna (m) above the node's Z coordinate",
                   DoubleValue (0),
                   MakeDoubleAccessor (&TwoRayGroundPropagationLossModel::m_heightAboveZ),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}

TwoRayGroundPropagationLossModel::TwoRayGroundPropagationLossModel ()
{
}
void
TwoRayGroundPropagationLossModel::SetSystemLoss (double systemLoss)
{
  m_systemLoss = systemLoss;
}
double
TwoRayGroundPropagationLossModel::GetSystemLoss (void) const
{
  return m_systemLoss;
}
void
TwoRayGroundPropagationLossModel::SetMinDistance (double minDistance)
{
  m_minDistance = minDistance;
}
double
TwoRayGroundPropagationLossModel::GetMinDistance (void) const
{
  return m_minDistance;
}
void
TwoRayGroundPropagationLossModel::SetHeightAboveZ (double heightAboveZ)
{
  m_heightAboveZ = heightAboveZ;
}

void
TwoRayGroundPropagationLossModel::SetFrequency (double frequency)
{
  m_frequency = frequency;
  static const double C = 299792458.0; // speed of light in vacuum
  m_lambda = C / frequency;
}

double
TwoRayGroundPropagationLossModel::GetFrequency (void) const
{
  return m_frequency;
}

double 
TwoRayGroundPropagationLossModel::DbmToW (double dbm) const
{
  double mw = std::pow (10.0,dbm / 10.0);
  return mw / 1000.0;
}

double
TwoRayGroundPropagationLossModel::DbmFromW (double w) const
{
  double dbm = std::log10 (w * 1000.0) * 10.0;
  return dbm;
}

double 
TwoRayGroundPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                                 Ptr<MobilityModel> a,
                                                 Ptr<MobilityModel> b) const
{
  /*
   * Two-Ray Ground equation:
   *
   * where Pt, Gt and Gr are in dBm units
   * L, Ht and Hr are in meter units.
   *
   *   Pr      Gt * Gr * (Ht^2 * Hr^2)
   *   -- =  (-------------------------)
   *   Pt            d^4 * L
   *
   * Gt: tx gain (unit-less)
   * Gr: rx gain (unit-less)
   * Pt: tx power (dBm)
   * d: distance (m)
   * L: system loss
   * Ht: Tx antenna height (m)
   * Hr: Rx antenna height (m)
   * lambda: wavelength (m)
   *
   * As with the Friis model we ignore tx and rx gain and output values
   * are in dB or dBm
   *
   *                      (Ht * Ht) * (Hr * Hr)
   * rx = tx + 10 log10 (-----------------------)
   *                      (d * d * d * d) * L
   */
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_minDistance)
    {
      return txPowerDbm;
    }

  // Set the height of the Tx and Rx antennae
  double txAntHeight = a->GetPosition ().z + m_heightAboveZ;
  double rxAntHeight = b->GetPosition ().z + m_heightAboveZ;

  // Calculate a crossover distance, under which we use Friis
  /*
   * 
   * dCross = (4 * pi * Ht * Hr) / lambda
   *
   */

  double dCross = (4 * PI * txAntHeight * rxAntHeight) / m_lambda;
  double tmp = 0;
  if (distance <= dCross)
    {
      // We use Friis
      double numerator = m_lambda * m_lambda;
      tmp = PI * distance;
      double denominator = 16 * tmp * tmp * m_systemLoss;
      double pr = 10 * std::log10 (numerator / denominator);
      NS_LOG_DEBUG ("Receiver within crossover (" << dCross << "m) for Two_ray path; using Friis");
      NS_LOG_DEBUG ("distance=" << distance << "m, attenuation coefficient=" << pr << "dB");
      return txPowerDbm + pr;
    }
  else   // Use Two-Ray Pathloss
    {
      tmp = txAntHeight * rxAntHeight;
      double rayNumerator = tmp * tmp;
      tmp = distance * distance;
      double rayDenominator = tmp * tmp * m_systemLoss;
      double rayPr = 10 * std::log10 (rayNumerator / rayDenominator);
      NS_LOG_DEBUG ("distance=" << distance << "m, attenuation coefficient=" << rayPr << "dB");
      return txPowerDbm + rayPr;

    }
}

int64_t
TwoRayGroundPropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (LogDistancePropagationLossModel)
  ;

TypeId
LogDistancePropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LogDistancePropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<LogDistancePropagationLossModel> ()
    .AddAttribute ("Exponent",
                   "The exponent of the Path Loss propagation model",
                   DoubleValue (3.0),
                   MakeDoubleAccessor (&LogDistancePropagationLossModel::m_exponent),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ReferenceDistance",
                   "The distance at which the reference loss is calculated (m)",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&LogDistancePropagationLossModel::m_referenceDistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ReferenceLoss",
                   "The reference loss at reference distance (dB). (Default is Friis at 1m with 5.15 GHz)",
                   DoubleValue (46.6777),
                   MakeDoubleAccessor (&LogDistancePropagationLossModel::m_referenceLoss),
                   MakeDoubleChecker<double> ())
  ;
  return tid;

}

LogDistancePropagationLossModel::LogDistancePropagationLossModel ()
{
}

void
LogDistancePropagationLossModel::SetPathLossExponent (double n)
{
  m_exponent = n;
}
void
LogDistancePropagationLossModel::SetReference (double referenceDistance, double referenceLoss)
{
  m_referenceDistance = referenceDistance;
  m_referenceLoss = referenceLoss;
}
double
LogDistancePropagationLossModel::GetPathLossExponent (void) const
{
  return m_exponent;
}

double
LogDistancePropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                                Ptr<MobilityModel> a,
                                                Ptr<MobilityModel> b) const
{
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_referenceDistance)
    {
      return txPowerDbm;
    }
  /**
   * The formula is:
   * rx = 10 * log (Pr0(tx)) - n * 10 * log (d/d0)
   *
   * Pr0: rx power at reference distance d0 (W)
   * d0: reference distance: 1.0 (m)
   * d: distance (m)
   * tx: tx power (dB)
   * rx: dB
   *
   * Which, in our case is:
   *
   * rx = rx0(tx) - 10 * n * log (d/d0)
   */
  double pathLossDb = 10 * m_exponent * std::log10 (distance / m_referenceDistance);
  double rxc = -m_referenceLoss - pathLossDb;
  NS_LOG_DEBUG ("distance="<<distance<<"m, reference-attenuation="<< -m_referenceLoss<<"dB, "<<
                "attenuation coefficient="<<rxc<<"db");
  return txPowerDbm + rxc;
}

int64_t
LogDistancePropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}


// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (ChannelBPropagationLossModel)
  ;
  
const double ChannelBPropagationLossModel::PI = 3.14159265358979323846;


TypeId
ChannelBPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ChannelBPropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<ChannelBPropagationLossModel> ()
    .AddAttribute ("Exponent",
                   "The exponent of the Path Loss propagation model",
                   DoubleValue (3.5),
                   MakeDoubleAccessor (&ChannelBPropagationLossModel::m_exponent),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ReferenceDistance",
                   "The distance at which the reference loss is calculated (m)",
                   DoubleValue (5.0),
                   MakeDoubleAccessor (&ChannelBPropagationLossModel::m_referenceDistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ReferenceLoss",
                   "The reference loss at reference distance (dB). (Default is Friis at 5m with 5 GHz)",
                   DoubleValue (60.396),
                   MakeDoubleAccessor (&ChannelBPropagationLossModel::m_referenceLoss),
                   MakeDoubleChecker<double> ())
	.AddAttribute ("Sfsdbrd",
                   "The shadow fading standard deviation before reference distance",
                   DoubleValue (5.0),
                   MakeDoubleAccessor (&ChannelBPropagationLossModel::m_sfsdbrd),
                   MakeDoubleChecker<double> ())
	.AddAttribute ("Sfsdard",
                   "The shadow fading standard deviation after reference distance)",
                   DoubleValue (5.0),
                   MakeDoubleAccessor (&ChannelBPropagationLossModel::m_sfsdard),
                   MakeDoubleChecker<double> ())
	.AddAttribute ("NormalRv",
                   "Access to the underlying NormalRandomVariable",
                   StringValue ("ns3::NormalRandomVariable"),
                   MakePointerAccessor (&ChannelBPropagationLossModel::m_normalRandomVariable),
                   MakePointerChecker<NormalRandomVariable> ())
	.AddAttribute ("Frequency", 
                   "The carrier frequency (in Hz) at which propagation occurs  (default is 5 GHz).",
                   DoubleValue (5.0e9),
                   MakeDoubleAccessor (&ChannelBPropagationLossModel::SetFrequency,
                                       &ChannelBPropagationLossModel::GetFrequency),
                   MakeDoubleChecker<double> ())
  ;
  return tid;

}

ChannelBPropagationLossModel::ChannelBPropagationLossModel ()
{
}

void
ChannelBPropagationLossModel::SetPathLossExponent (double n)
{
  m_exponent = n;
}
void
ChannelBPropagationLossModel::SetReference (double referenceDistance, double referenceLoss)
{
  m_referenceDistance = referenceDistance;
  m_referenceLoss = referenceLoss;
}
double
ChannelBPropagationLossModel::GetPathLossExponent (void) const
{
  return m_exponent;
}

void
ChannelBPropagationLossModel::SetFrequency (double frequency)
{
  m_frequency = frequency;
  static const double C = 299792458.0; // speed of light in vacuum
  m_lambda = C / frequency;
  if (frequency == 5.0e9)
  {
	m_referenceLoss = 60.396;
  }
  else if (frequency == 2.4e9)
  {
	m_referenceLoss = 54;
  }
}

double
ChannelBPropagationLossModel::GetFrequency (void) const
{
  return m_frequency;
}

double 
ChannelBPropagationLossModel::shadowing_factor(double mu, double sigma) const
{
    double U1, U2, W, mult;
    static double X1, X2;
    static int call = 0;

    if (call == 1)
    {
        call = !call;
        return (mu + sigma * (double) X2);
    }

    do
    {
        U1 = -1 + ((double) rand () / RAND_MAX) * 2;
        U2 = -1 + ((double) rand () / RAND_MAX) * 2;
        W = pow (U1, 2) + pow (U2, 2);
    }while (W >= 1 || W == 0);

    mult = sqrt ((-2 * log (W)) / W);
    X1 = U1 * mult;
    X2 = U2 * mult;

    call = !call;
    //printf("%f\n",mu + sigma * (double) X1);
    return (mu + sigma * (double) X1);
}

double
ChannelBPropagationLossModel::CalcRxPowerforTest (double txPowerDbm,
                                                double distance)
{
  //double distance = a->GetDistanceFrom (b);
  if (distance <= m_referenceDistance)
    {
	  //double shadowFading = m_normalRandomVariable->GetValue (0, m_sfsdbrd);
	  double shadowFading = shadowing_factor(0.0, m_sfsdbrd);
	  double numerator = m_lambda * m_lambda;
	  double denominator = 16 * PI * PI * distance * distance;
      double pathLossDb = 10 * std::log10 (numerator / denominator);
	  double rxc = pathLossDb + shadowFading;
	  NS_LOG_DEBUG ("distance="<<distance<<"attenuation coefficient="<<rxc<<"db");
      return txPowerDbm + rxc;
    }
  /**
   * The formula is:
   * rx = 10 * log (Pr0(tx)) - n * 10 * log (d/d0)
   *
   * Pr0: rx power at reference distance d0 (W)
   * d0: reference distance: 5.0 (m)
   * d: distance (m)
   * tx: tx power (dB)
   * rx: dB
   *
   * Which, in our case is:
   *
   * rx = rx0(tx) - 10 * n * log (d/d0)
   *
   * for distances less than the reference distance d0, we use
   * the free space propagation loss model which works as follows - 
   *
   *                           lambda^2
   * rx = tx +  10 log10 (-------------------)
   *                       (4 * pi * d)^2 
   *
   * rx: rx power (dB)
   * tx: tx power (dB)
   * d: distance (m)
   * lambda: wavelength (m)
   */
  double pathLossDb = 10 * m_exponent * std::log10 (distance / m_referenceDistance);
  //double shadowFading = m_normalRandomVariable->GetValue (0, m_sfsdard);
  double shadowFading = shadowing_factor(0.0, m_sfsdard);
  double rxc = -m_referenceLoss - pathLossDb + shadowFading;
  NS_LOG_DEBUG ("distance="<<distance<<"m, reference-attenuation="<< -m_referenceLoss<<"dB, "<<
                "attenuation coefficient="<<rxc<<"db");
  return txPowerDbm + rxc;
}


double
ChannelBPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                                Ptr<MobilityModel> a,
                                                Ptr<MobilityModel> b) const
{
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_referenceDistance)
    {
	  //double shadowFading = m_normalRandomVariable->GetValue (0, m_sfsdbrd);
	  double shadowFading = shadowing_factor(0, m_sfsdbrd);
	  double numerator = m_lambda * m_lambda;
	  double denominator = 16 * PI * PI * distance * distance;
      double pathLossDb = 10 * std::log10 (numerator / denominator);
	  double rxc = pathLossDb + shadowFading;
	  NS_LOG_DEBUG ("distance="<<distance<<"attenuation coefficient="<<rxc<<"db");
      return txPowerDbm + rxc;
    }
  /**
   * The formula is:
   * rx = 10 * log (Pr0(tx)) - n * 10 * log (d/d0)
   *
   * Pr0: rx power at reference distance d0 (W)
   * d0: reference distance: 5.0 (m)
   * d: distance (m)
   * tx: tx power (dB)
   * rx: dB
   *
   * Which, in our case is:
   *
   * rx = rx0(tx) - 10 * n * log (d/d0)
   *
   * for distances less than the reference distance d0, we use
   * the free space propagation loss model which works as follows - 
   *
   *                           lambda^2
   * rx = tx +  10 log10 (-------------------)
   *                       (4 * pi * d)^2 
   *
   * rx: rx power (dB)
   * tx: tx power (dB)
   * d: distance (m)
   * lambda: wavelength (m)
   */
  double pathLossDb = 10 * m_exponent * std::log10 (distance / m_referenceDistance);
  //double shadowFading = m_normalRandomVariable->GetValue (0, m_sfsdard);
  double shadowFading = shadowing_factor(0, m_sfsdard);
  double rxc = -m_referenceLoss - pathLossDb + shadowFading;
  NS_LOG_DEBUG ("distance="<<distance<<"m, reference-attenuation="<< -m_referenceLoss<<"dB, "<<
                "attenuation coefficient="<<rxc<<"db");
  return txPowerDbm + rxc;
}

int64_t
ChannelBPropagationLossModel::DoAssignStreams (int64_t stream)
{
  m_normalRandomVariable->SetStream (stream);
  return 1;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (OutdoorPropagationLossModel)
  ;
  
const double OutdoorPropagationLossModel::PI = 3.14159265358979323846;


TypeId
OutdoorPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::OutdoorPropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<OutdoorPropagationLossModel> ()
    .AddAttribute ("ReferenceDistance",
                   "The distance at which the reference loss is calculated (m)",
                   DoubleValue (300.0),
                   MakeDoubleAccessor (&OutdoorPropagationLossModel::m_referenceDistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("isLOS",
                   "if the link is LOS or NLOS",
                   BooleanValue (true),
                   MakeBooleanAccessor (&OutdoorPropagationLossModel::m_islos),
                   MakeBooleanChecker ())
	.AddAttribute ("hBS",
                   "antenna height 1",
                   DoubleValue (10.0),
                   MakeDoubleAccessor (&OutdoorPropagationLossModel::m_hbs),
                   MakeDoubleChecker<double> ())
	.AddAttribute ("hMS",
                   "antenna height 2",
                   DoubleValue (1.5),
                   MakeDoubleAccessor (&OutdoorPropagationLossModel::m_hms),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("NormalRv",
                   "Access to the underlying NormalRandomVariable",
                   StringValue ("ns3::NormalRandomVariable"),
                   MakePointerAccessor (&OutdoorPropagationLossModel::m_normalRandomVariable),
                   MakePointerChecker<NormalRandomVariable> ())
	.AddAttribute ("Frequency", 
                   "The carrier frequency (in Hz) at which propagation occurs  (default is 5 GHz).",
                   DoubleValue (5.0e9),
                   MakeDoubleAccessor (&OutdoorPropagationLossModel::SetFrequency,
                                       &OutdoorPropagationLossModel::GetFrequency),
                   MakeDoubleChecker<double> ())
  ;
  return tid;

}

OutdoorPropagationLossModel::OutdoorPropagationLossModel ()
{
}

void
OutdoorPropagationLossModel::SethBS (double hbs)
{
  m_hbs = hbs;
  static const double C = 300000000.0; // speed of light in vacuum
  m_referenceDistance = (4*(m_hbs-1.0)*(m_hms-1.0)*m_frequency) / C;
}

void
OutdoorPropagationLossModel::SethMS (double hms)
{
  m_hms = hms;
  static const double C = 300000000.0; // speed of light in vacuum
  m_referenceDistance = (4*(m_hbs-1.0)*(m_hms-1.0)*m_frequency) / C;
}

void
OutdoorPropagationLossModel::SetLOS (bool los)
{
  m_islos = los;
}

void
OutdoorPropagationLossModel::SetFrequency (double frequency)
{
  m_frequency = frequency;
  static const double C = 300000000.0; // speed of light in vacuum
  m_lambda = C / frequency;
  m_referenceDistance = (4*(m_hbs-1.0)*(m_hms-1.0)*m_frequency) / C;
}

double
OutdoorPropagationLossModel::GetFrequency (void) const
{
  return m_frequency;
}

double
OutdoorPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                                Ptr<MobilityModel> a,
                                                Ptr<MobilityModel> b) const
{
  double distance = a->GetDistanceFrom (b);
  
  if(m_islos) {
	  if (distance <= m_referenceDistance)
		{
		  double pathLossDb = (22.0 * std::log10 (distance)) + 28.0 + (20.0 * std::log10 (m_frequency / 1000000000.0));
		  double rxc = - pathLossDb;
		  NS_LOG_DEBUG ("distance="<<distance<<"attenuation coefficient="<<rxc<<"db");
		  return txPowerDbm + rxc;
		}
	  double pathLossDb = (40.0 * std::log10 (distance)) + 7.8 - (18.0 * std::log10 (m_hbs-1.0)) - (18.0 * std::log10 (m_hms-1.0)) + (2.0 * std::log10 (m_frequency / 1000000000.0));
	  double shadowFading = m_normalRandomVariable->GetValue (0, 3);
	  double rxc = - pathLossDb - shadowFading;
	  NS_LOG_DEBUG ("distance="<<distance<<"attenuation coefficient="<<rxc<<"db");
	  return txPowerDbm + rxc;
  }
  double pathLossDb = (36.7 * std::log10 (distance)) + 22.7 + (26.0 * std::log10 (m_frequency / 1000000000.0));
  double shadowFading = m_normalRandomVariable->GetValue (0, 4);
  double rxc = - pathLossDb - shadowFading;
  NS_LOG_DEBUG ("distance="<<distance<<"attenuation coefficient="<<rxc<<"db");
  return txPowerDbm + rxc;
}

double
OutdoorPropagationLossModel::DoCalcRxPower_STA_STA (double txPowerDbm,
                                                Ptr<MobilityModel> a,
                                                Ptr<MobilityModel> b) 
{	
  SethBS (1.5);
  SethMS (1.5);
  
  double distance = a->GetDistanceFrom (b);
  double p_los = (std::min (18.0/distance, 1.0) * (1.0 - exp(-distance/36.0))) + exp(-distance/36.0);
   if (m_random->GetValue () < p_los)
		SetLOS(true);
   else
		SetLOS(false);
  
  return DoCalcRxPower (txPowerDbm, a, b);
}

double
OutdoorPropagationLossModel::DoCalcRxPower_STA_AP (double txPowerDbm,
                                                Ptr<MobilityModel> a,
                                                Ptr<MobilityModel> b)
{
  SethBS (10.0);
  SethMS (1.5);
  
  double distance = a->GetDistanceFrom (b);
  double p_los = (std::min (18.0/distance, 1.0) * (1.0 - exp(-distance/36.0))) + exp(-distance/36.0);
   if (m_random->GetValue () < p_los)
		SetLOS(true);
   else
		SetLOS(false);
  
  return DoCalcRxPower (txPowerDbm, a, b);
}

double
OutdoorPropagationLossModel::DoCalcRxPower_AP_AP (double txPowerDbm,
                                                Ptr<MobilityModel> a,
                                                Ptr<MobilityModel> b)
{
  SethBS (10.0);
  SethMS (10.0);
  
  double distance = a->GetDistanceFrom (b);
  double p_los = (std::min (18.0/distance, 1.0) * (1.0 - exp(-distance/36.0))) + exp(-distance/36.0);
   if (m_random->GetValue () < p_los)
		SetLOS(true);
   else
		SetLOS(false);
  
  return DoCalcRxPower (txPowerDbm, a, b);
}


int64_t
OutdoorPropagationLossModel::DoAssignStreams (int64_t stream)
{
  m_random->SetStream (stream);
  return 1;
}


// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (ChannelDPropagationLossModel)
  ;
  
const double ChannelDPropagationLossModel::PI = 3.14159265358979323846;


TypeId
ChannelDPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ChannelDPropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<ChannelDPropagationLossModel> ()
    .AddAttribute ("Exponent",
                   "The exponent of the Path Loss propagation model",
                   DoubleValue (3.5),
                   MakeDoubleAccessor (&ChannelDPropagationLossModel::m_exponent),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ReferenceDistance",
                   "The distance at which the reference loss is calculated (m)",
                   DoubleValue (10.0),
                   MakeDoubleAccessor (&ChannelDPropagationLossModel::m_referenceDistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ReferenceLoss",
                   "The reference loss at reference distance (dB). (Default is Friis at 10m with 5 GHz)",
                   DoubleValue (66.417),
                   MakeDoubleAccessor (&ChannelDPropagationLossModel::m_referenceLoss),
                   MakeDoubleChecker<double> ())
	.AddAttribute ("Sfsdbrd",
                   "The shadow fading standard deviation before reference distance",
                   DoubleValue (5.0),
                   MakeDoubleAccessor (&ChannelDPropagationLossModel::m_sfsdbrd),
                   MakeDoubleChecker<double> ())
	.AddAttribute ("Sfsdard",
                   "The shadow fading standard deviation after reference distance)",
                   DoubleValue (5.0),
                   MakeDoubleAccessor (&ChannelDPropagationLossModel::m_sfsdard),
                   MakeDoubleChecker<double> ())
	.AddAttribute ("NormalRv",
                   "Access to the underlying NormalRandomVariable",
                   StringValue ("ns3::NormalRandomVariable"),
                   MakePointerAccessor (&ChannelDPropagationLossModel::m_normalRandomVariable),
                   MakePointerChecker<NormalRandomVariable> ())
	.AddAttribute ("Frequency", 
                   "The carrier frequency (in Hz) at which propagation occurs  (default is 5 GHz).",
                   DoubleValue (5.0e9),
                   MakeDoubleAccessor (&ChannelDPropagationLossModel::SetFrequency,
                                       &ChannelDPropagationLossModel::GetFrequency),
                   MakeDoubleChecker<double> ())
  ;
  return tid;

}

ChannelDPropagationLossModel::ChannelDPropagationLossModel ()
{
}

void
ChannelDPropagationLossModel::SetPathLossExponent (double n)
{
  m_exponent = n;
}
void
ChannelDPropagationLossModel::SetReference (double referenceDistance, double referenceLoss)
{
  m_referenceDistance = referenceDistance;
  m_referenceLoss = referenceLoss;
}
double
ChannelDPropagationLossModel::GetPathLossExponent (void) const
{
  return m_exponent;
}

void
ChannelDPropagationLossModel::SetFrequency (double frequency)
{
  m_frequency = frequency;
  static const double C = 299792458.0; // speed of light in vacuum
  m_lambda = C / frequency;
  if (frequency == 5.0e9)
  {
	m_referenceLoss = 66.417;
  }
  else if (frequency == 2.4e9)
  {
	m_referenceLoss = 60.04;
  }
}

double 
ChannelDPropagationLossModel::shadowing_factor(double mu, double sigma) const
{
    double U1, U2, W, mult;
    static double X1, X2;
    static int call = 0;

    if (call == 1)
    {
        call = !call;
        return (mu + sigma * (double) X2);
    }

    do
    {
        U1 = -1 + ((double) rand () / RAND_MAX) * 2;
        U2 = -1 + ((double) rand () / RAND_MAX) * 2;
        W = pow (U1, 2) + pow (U2, 2);
    }while (W >= 1 || W == 0);

    mult = sqrt ((-2 * log (W)) / W);
    X1 = U1 * mult;
    X2 = U2 * mult;

    call = !call;
    //printf("%f\n",mu + sigma * (double) X1);
    return (mu + sigma * (double) X1);
}

double
ChannelDPropagationLossModel::GetFrequency (void) const
{
  return m_frequency;
}

double
ChannelDPropagationLossModel::CalcRxPowerforTest (double txPowerDbm,
                                                double distance)
{
  //double distance = a->GetDistanceFrom (b);
  if (distance <= m_referenceDistance)
    {
	  //double shadowFading = m_normalRandomVariable->GetValue (0, m_sfsdbrd);
	  double shadowFading = shadowing_factor(0, m_sfsdbrd);
	  double numerator = m_lambda * m_lambda;
	  double denominator = 16 * PI * PI * distance * distance;
      double pathLossDb = 10 * std::log10 (numerator / denominator);
	  double rxc = pathLossDb + shadowFading;
	  NS_LOG_DEBUG ("distance="<<distance<<"attenuation coefficient="<<rxc<<"db");
      return txPowerDbm + rxc;
    }
  /**
   * The formula is:
   * rx = 10 * log (Pr0(tx)) - n * 10 * log (d/d0)
   *
   * Pr0: rx power at reference distance d0 (W)
   * d0: reference distance: 5.0 (m)
   * d: distance (m)
   * tx: tx power (dB)
   * rx: dB
   *
   * Which, in our case is:
   *
   * rx = rx0(tx) - 10 * n * log (d/d0)
   *
   * for distances less than the reference distance d0, we use
   * the free space propagation loss model which works as follows - 
   *
   *                           lambda^2
   * rx = tx +  10 log10 (-------------------)
   *                       (4 * pi * d)^2 
   *
   * rx: rx power (dB)
   * tx: tx power (dB)
   * d: distance (m)
   * lambda: wavelength (m)
   */
  double pathLossDb = 10 * m_exponent * std::log10 (distance / m_referenceDistance);
  //double shadowFading = m_normalRandomVariable->GetValue (0, m_sfsdard);
  double shadowFading = shadowing_factor(0, m_sfsdard);
  double rxc = -m_referenceLoss - pathLossDb + shadowFading;
  NS_LOG_DEBUG ("distance="<<distance<<"m, reference-attenuation="<< -m_referenceLoss<<"dB, "<<
                "attenuation coefficient="<<rxc<<"db");
  return txPowerDbm + rxc;
}


double
ChannelDPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                                Ptr<MobilityModel> a,
                                                Ptr<MobilityModel> b) const
{
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_referenceDistance)
    {
	  //double shadowFading = m_normalRandomVariable->GetValue (0, m_sfsdbrd);
	  double shadowFading = shadowing_factor(0, m_sfsdbrd);
	  double numerator = m_lambda * m_lambda;
	  double denominator = 16 * PI * PI * distance * distance;
      double pathLossDb = 10 * std::log10 (numerator / denominator);
	  double rxc = pathLossDb + shadowFading;
	  NS_LOG_DEBUG ("distance="<<distance<<"attenuation coefficient="<<rxc<<"db");
      return txPowerDbm + rxc;
    }
  /**
   * The formula is:
   * rx = 10 * log (Pr0(tx)) - n * 10 * log (d/d0)
   *
   * Pr0: rx power at reference distance d0 (W)
   * d0: reference distance: 5.0 (m)
   * d: distance (m)
   * tx: tx power (dB)
   * rx: dB
   *
   * Which, in our case is:
   *
   * rx = rx0(tx) - 10 * n * log (d/d0)
   *
   * for distances less than the reference distance d0, we use
   * the free space propagation loss model which works as follows - 
   *
   *                           lambda^2
   * rx = tx +  10 log10 (-------------------)
   *                       (4 * pi * d)^2 
   *
   * rx: rx power (dB)
   * tx: tx power (dB)
   * d: distance (m)
   * lambda: wavelength (m)
   */
  double pathLossDb = 10 * m_exponent * std::log10 (distance / m_referenceDistance);
  //double shadowFading = m_normalRandomVariable->GetValue (0, m_sfsdard);
  double shadowFading = shadowing_factor(0, m_sfsdard);
  double rxc = -m_referenceLoss - pathLossDb + shadowFading;
  NS_LOG_DEBUG ("distance="<<distance<<"m, reference-attenuation="<< -m_referenceLoss<<"dB, "<<
                "attenuation coefficient="<<rxc<<"db");
  return txPowerDbm + rxc;
}

int64_t
ChannelDPropagationLossModel::DoAssignStreams (int64_t stream)
{
  m_normalRandomVariable->SetStream (stream);
  return 1;
}

// ------------------------------------------------------------------------- //


NS_OBJECT_ENSURE_REGISTERED (ThreeLogDistancePropagationLossModel)
  ;

TypeId
ThreeLogDistancePropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ThreeLogDistancePropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<ThreeLogDistancePropagationLossModel> ()
    .AddAttribute ("Distance0",
                   "Beginning of the first (near) distance field",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_distance0),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Distance1",
                   "Beginning of the second (middle) distance field.",
                   DoubleValue (200.0),
                   MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_distance1),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Distance2",
                   "Beginning of the third (far) distance field.",
                   DoubleValue (500.0),
                   MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_distance2),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Exponent0",
                   "The exponent for the first field.",
                   DoubleValue (1.9),
                   MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_exponent0),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Exponent1",
                   "The exponent for the second field.",
                   DoubleValue (3.8),
                   MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_exponent1),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Exponent2",
                   "The exponent for the third field.",
                   DoubleValue (3.8),
                   MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_exponent2),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ReferenceLoss",
                   "The reference loss at distance d0 (dB). (Default is Friis at 1m with 5 GHz)",
                   DoubleValue (60.396),
                   MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_referenceLoss),
                   MakeDoubleChecker<double> ())
  ;
  return tid;

}

ThreeLogDistancePropagationLossModel::ThreeLogDistancePropagationLossModel ()
{
}

double 
ThreeLogDistancePropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                                     Ptr<MobilityModel> a,
                                                     Ptr<MobilityModel> b) const
{
  double distance = a->GetDistanceFrom (b);
  NS_ASSERT (distance >= 0);

  // See doxygen comments for the formula and explanation

  double pathLossDb;

  if (distance < m_distance0)
    {
      pathLossDb = 0;
    }
  else if (distance < m_distance1)
    {
      pathLossDb = m_referenceLoss
        + 10 * m_exponent0 * std::log10 (distance / m_distance0);
    }
  else if (distance < m_distance2)
    {
      pathLossDb = m_referenceLoss
        + 10 * m_exponent0 * std::log10 (m_distance1 / m_distance0)
        + 10 * m_exponent1 * std::log10 (distance / m_distance1);
    }
  else
    {
      pathLossDb = m_referenceLoss
        + 10 * m_exponent0 * std::log10 (m_distance1 / m_distance0)
        + 10 * m_exponent1 * std::log10 (m_distance2 / m_distance1)
        + 10 * m_exponent2 * std::log10 (distance / m_distance2);
    }

  NS_LOG_DEBUG ("ThreeLogDistance distance=" << distance << "m, " <<
                "attenuation=" << pathLossDb << "dB");

  return txPowerDbm - pathLossDb;
}

int64_t
ThreeLogDistancePropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (NakagamiPropagationLossModel)
  ;

TypeId
NakagamiPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NakagamiPropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<NakagamiPropagationLossModel> ()
    .AddAttribute ("Distance1",
                   "Beginning of the second distance field. Default is 80m.",
                   DoubleValue (80.0),
                   MakeDoubleAccessor (&NakagamiPropagationLossModel::m_distance1),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Distance2",
                   "Beginning of the third distance field. Default is 200m.",
                   DoubleValue (200.0),
                   MakeDoubleAccessor (&NakagamiPropagationLossModel::m_distance2),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("m0",
                   "m0 for distances smaller than Distance1. Default is 1.5.",
                   DoubleValue (1.5),
                   MakeDoubleAccessor (&NakagamiPropagationLossModel::m_m0),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("m1",
                   "m1 for distances smaller than Distance2. Default is 0.75.",
                   DoubleValue (0.75),
                   MakeDoubleAccessor (&NakagamiPropagationLossModel::m_m1),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("m2",
                   "m2 for distances greater than Distance2. Default is 0.75.",
                   DoubleValue (0.75),
                   MakeDoubleAccessor (&NakagamiPropagationLossModel::m_m2),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ErlangRv",
                   "Access to the underlying ErlangRandomVariable",
                   StringValue ("ns3::ErlangRandomVariable"),
                   MakePointerAccessor (&NakagamiPropagationLossModel::m_erlangRandomVariable),
                   MakePointerChecker<ErlangRandomVariable> ())
    .AddAttribute ("GammaRv",
                   "Access to the underlying GammaRandomVariable",
                   StringValue ("ns3::GammaRandomVariable"),
                   MakePointerAccessor (&NakagamiPropagationLossModel::m_gammaRandomVariable),
                   MakePointerChecker<GammaRandomVariable> ());
  ;
  return tid;

}

NakagamiPropagationLossModel::NakagamiPropagationLossModel ()
{
}

double
NakagamiPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                             Ptr<MobilityModel> a,
                                             Ptr<MobilityModel> b) const
{
  // select m parameter

  double distance = a->GetDistanceFrom (b);
  NS_ASSERT (distance >= 0);

  double m;
  if (distance < m_distance1)
    {
      m = m_m0;
    }
  else if (distance < m_distance2)
    {
      m = m_m1;
    }
  else
    {
      m = m_m2;
    }

  // the current power unit is dBm, but Watt is put into the Nakagami /
  // Rayleigh distribution.
  double powerW = std::pow (10, (txPowerDbm - 30) / 10);

  double resultPowerW;

  // switch between Erlang- and Gamma distributions: this is only for
  // speed. (Gamma is equal to Erlang for any positive integer m.)
  unsigned int int_m = static_cast<unsigned int>(std::floor (m));

  if (int_m == m)
    {
      resultPowerW = m_erlangRandomVariable->GetValue (int_m, powerW / m);
    }
  else
    {
      resultPowerW = m_gammaRandomVariable->GetValue (m, powerW / m);
    }

  double resultPowerDbm = 10 * std::log10 (resultPowerW) + 30;

  NS_LOG_DEBUG ("Nakagami distance=" << distance << "m, " <<
                "power=" << powerW <<"W, " <<
                "resultPower=" << resultPowerW << "W=" << resultPowerDbm << "dBm");

  return resultPowerDbm;
}

int64_t
NakagamiPropagationLossModel::DoAssignStreams (int64_t stream)
{
  m_erlangRandomVariable->SetStream (stream);
  m_gammaRandomVariable->SetStream (stream + 1);
  return 2;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (FixedRssLossModel)
  ;

TypeId 
FixedRssLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FixedRssLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<FixedRssLossModel> ()
    .AddAttribute ("Rss", "The fixed receiver Rss.",
                   DoubleValue (-150.0),
                   MakeDoubleAccessor (&FixedRssLossModel::m_rss),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}
FixedRssLossModel::FixedRssLossModel ()
  : PropagationLossModel ()
{
}

FixedRssLossModel::~FixedRssLossModel ()
{
}

void
FixedRssLossModel::SetRss (double rss)
{
  m_rss = rss;
}

double
FixedRssLossModel::DoCalcRxPower (double txPowerDbm,
                                  Ptr<MobilityModel> a,
                                  Ptr<MobilityModel> b) const
{
  return m_rss;
}

int64_t
FixedRssLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (MatrixPropagationLossModel)
  ;

TypeId 
MatrixPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MatrixPropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<MatrixPropagationLossModel> ()
    .AddAttribute ("DefaultLoss", "The default value for propagation loss, dB.",
                   DoubleValue (std::numeric_limits<double>::max ()),
                   MakeDoubleAccessor (&MatrixPropagationLossModel::m_default),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}

MatrixPropagationLossModel::MatrixPropagationLossModel ()
  : PropagationLossModel (), m_default (std::numeric_limits<double>::max ())
{
}

MatrixPropagationLossModel::~MatrixPropagationLossModel ()
{
}

void 
MatrixPropagationLossModel::SetDefaultLoss (double loss)
{
  m_default = loss;
}

void
MatrixPropagationLossModel::SetLoss (Ptr<MobilityModel> ma, Ptr<MobilityModel> mb, double loss, bool symmetric)
{
  NS_ASSERT (ma != 0 && mb != 0);

  MobilityPair p = std::make_pair (ma, mb);
  std::map<MobilityPair, double>::iterator i = m_loss.find (p);

  if (i == m_loss.end ())
    {
      m_loss.insert (std::make_pair (p, loss));
    }
  else
    {
      i->second = loss;
    }

  if (symmetric)
    {
      SetLoss (mb, ma, loss, false);
    }
}

double 
MatrixPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                           Ptr<MobilityModel> a,
                                           Ptr<MobilityModel> b) const
{
  std::map<MobilityPair, double>::const_iterator i = m_loss.find (std::make_pair (a, b));

  if (i != m_loss.end ())
    {
      return txPowerDbm - i->second;
    }
  else
    {
      return txPowerDbm - m_default;
    }
}

int64_t
MatrixPropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (RangePropagationLossModel)
  ;

TypeId
RangePropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RangePropagationLossModel")
    .SetParent<PropagationLossModel> ()
    .AddConstructor<RangePropagationLossModel> ()
    .AddAttribute ("MaxRange",
                   "Maximum Transmission Range (meters)",
                   DoubleValue (250),
                   MakeDoubleAccessor (&RangePropagationLossModel::m_range),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}

RangePropagationLossModel::RangePropagationLossModel ()
{
}

double
RangePropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                          Ptr<MobilityModel> a,
                                          Ptr<MobilityModel> b) const
{
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_range)
    {
      return txPowerDbm;
    }
  else
    {
      return -1000;
    }
}

int64_t
RangePropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

} // namespace ns3
