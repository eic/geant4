//
// ********************************************************************
// * DISCLAIMER                                                       *
// *                                                                  *
// * The following disclaimer summarizes all the specific disclaimers *
// * of contributors to this software. The specific disclaimers,which *
// * govern, are listed with their locations in:                      *
// *   http://cern.ch/geant4/license                                  *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.                                                             *
// *                                                                  *
// * This  code  implementation is the  intellectual property  of the *
// * GEANT4 collaboration.                                            *
// * By copying,  distributing  or modifying the Program (or any work *
// * based  on  the Program)  you indicate  your  acceptance of  this *
// * statement, and all its terms.                                    *
// ********************************************************************
//
#include "G4MesonAbsorption.hh"
#include "G4LorentzRotation.hh"
#include "G4LorentzVector.hh"
#include "Randomize.hh"
#include "G4KineticTrackVector.hh"
#include "G4CollisionInitialState.hh"
#include <cmath>
#include "G4PionPlus.hh"
#include "G4PionMinus.hh"
#include "G4ParticleDefinition.hh"
#include "G4HadTmpUtil.hh"

// first prototype

const std::vector<G4CollisionInitialState *> & G4MesonAbsorption::
GetCollisions(G4KineticTrack * aProjectile, 
              std::vector<G4KineticTrack *> & someCandidates,
	      G4double aCurrentTime)
{
  theCollisions.clear();
  if(someCandidates.size() >1)
  {
    std::vector<G4KineticTrack *>::iterator j=someCandidates.begin();
    for(; j != someCandidates.end(); ++j)
    {
      G4double collisionTime = GetTimeToAbsorption(*aProjectile, **j);
      if(collisionTime == DBL_MAX) 
      {
        continue;
      }
      G4KineticTrackVector aTarget;
      aTarget.push_back(*j);
      FindAndFillCluster(aTarget, aProjectile, someCandidates);
      if(aTarget.size()>=2)
      {
        theCollisions.push_back(
        new G4CollisionInitialState(collisionTime+aCurrentTime, aProjectile, aTarget, this) );
      }
    }
  }
  return theCollisions;
}


void G4MesonAbsorption::
FindAndFillCluster(G4KineticTrackVector & result, 
                   G4KineticTrack * aProjectile, std::vector<G4KineticTrack *> & someCandidates)
{
  std::vector<G4KineticTrack *>::iterator j=someCandidates.begin();
  G4KineticTrack * aTarget = result[0];
  G4int chargeSum = G4lrint(aTarget->GetDefinition()->GetPDGCharge());
  chargeSum+=G4lrint(aProjectile->GetDefinition()->GetPDGCharge());
  G4ThreeVector firstBase = aTarget->GetPosition();
  G4double min = DBL_MAX;
  G4KineticTrack * partner = 0;
  for(; j != someCandidates.end(); ++j)
  {
    if(*j == aTarget) continue;
    G4int cCharge = G4lrint((*j)->GetDefinition()->GetPDGCharge());
    if (chargeSum+cCharge > 2) continue;
    if (chargeSum+cCharge < 0) continue;
    // get the one with the smallest distance. 
    G4ThreeVector secodeBase = (*j)->GetPosition();
    if((firstBase+secodeBase).mag()<min)
    {
      min=(firstBase+secodeBase).mag();
      partner = *j;
    }
  }
  if(partner) result.push_back(partner);
  else result.clear();
}


G4KineticTrackVector * G4MesonAbsorption::
GetFinalState(G4KineticTrack * projectile, 
              std::vector<G4KineticTrack *> & targets)
{
  // G4cout << "We have an absorption !!!!!!!!!!!!!!!!!!!!!!"<<G4endl;
  // Only 2-body absorption for the time being.
  // If insufficient, use 2-body absorption and clusterization to emulate 3-,4-,etc body absorption.
  G4LorentzVector thePro = projectile->Get4Momentum();
  G4LorentzVector theT1 = targets[0]->Get4Momentum();
  G4LorentzVector theT2 = targets[1]->Get4Momentum();
  G4LorentzVector incoming = thePro + theT1 + theT2;
  G4double energyBalance = incoming.t();
  G4int chargeBalance = G4lrint(projectile->GetDefinition()->GetPDGCharge()
                       + targets[0]->GetDefinition()->GetPDGCharge()
                       + targets[1]->GetDefinition()->GetPDGCharge());
		       
  G4int baryonBalance = projectile->GetDefinition()->GetBaryonNumber()
                       + targets[0]->GetDefinition()->GetBaryonNumber()
                       + targets[1]->GetDefinition()->GetBaryonNumber();
   
  
  // boost all to MMS
  G4LorentzRotation toSPS((-1)*(thePro + theT1 + theT2).boostVector());
  theT1 = toSPS * theT1;
  theT2 = toSPS * theT2;
  thePro = toSPS * thePro;
  G4LorentzRotation fromSPS(toSPS.inverse());
 
  // rotate to z
  G4LorentzRotation toZ;
  G4LorentzVector Ptmp=projectile->Get4Momentum();
  toZ.rotateZ(-1*Ptmp.phi());
  toZ.rotateY(-1*Ptmp.theta());
  theT1 = toZ * theT1;
  theT2 = toZ * theT2;
  thePro = toZ * thePro;
  G4LorentzRotation toLab(toZ.inverse());
  
  // Get definitions
  G4ParticleDefinition * d1 = targets[0]->GetDefinition();
  G4ParticleDefinition * d2 = targets[1]->GetDefinition();
  if(0.5>G4UniformRand())
  {
    G4ParticleDefinition * temp;
    temp=d1;d1=d2;d2=temp;
  }
  G4ParticleDefinition * dp = projectile->GetDefinition();
  if(dp->GetPDGCharge()<-.5)
  {
    if(d1->GetPDGCharge()>.5)
    {
      if(d2->GetPDGCharge()>.5 && 0.5>G4UniformRand())
      {
        d2 = G4Neutron::NeutronDefinition();
      }
      else
      {
        d1 = G4Neutron::NeutronDefinition();
      }
    }
    else if(d2->GetPDGCharge()>.5)
    {
      d2 = G4Neutron::NeutronDefinition();  
    }
  }
  else if(dp->GetPDGCharge()>.5)
  {
    if(d1->GetPDGCharge()<.5)
    {
      if(d2->GetPDGCharge()<.5 && 0.5>G4UniformRand())
      {
        d2 = G4Proton::ProtonDefinition();
      }
      else
      {
        d1 = G4Proton::ProtonDefinition();
      }
    }
    else if(d2->GetPDGCharge()<.5)
    {
      d2 = G4Proton::ProtonDefinition();  
    }
  }
  
  // calculate the momenta.
  G4double M = (thePro+theT1+theT2).mag();
  G4double m1 = d1->GetPDGMass();
  G4double m2 = d2->GetPDGMass();
  G4double m = sqrt(M*M-m1*m1-m2*m2);
  G4double p = sqrt((m*m*m*m - 4.*m1*m1 * m2*m2)/(4.*(M*M)));
  G4double costh = 2.*G4UniformRand()-1.;
  G4double phi = 2.*pi*G4UniformRand();
  G4ThreeVector pFinal(p*sin(acos(costh))*cos(phi), p*sin(acos(costh))*sin(phi), p*costh);
  
  // G4cout << "testing p "<<p-pFinal.mag()<<G4endl;
  // construct the final state particles lorentz momentum.
  G4double eFinal1 = sqrt(m1*m1+pFinal.mag2());
  G4LorentzVector final1(pFinal, eFinal1);
  G4double eFinal2 = sqrt(m2*m2+pFinal.mag2());
  G4LorentzVector final2(-1.*pFinal, eFinal2);
  
  // rotate back.
  final1 = toLab * final1;
  final2 = toLab * final2;
  
  // boost back to LAB
  final1 = fromSPS * final1;
  final2 = fromSPS * final2;
  
  // make the final state
  G4KineticTrack * f1 = 
      new G4KineticTrack(d1, 0., targets[0]->GetPosition(), final1);
  G4KineticTrack * f2 = 
      new G4KineticTrack(d2, 0., targets[1]->GetPosition(), final2);
  G4KineticTrackVector * result = new G4KineticTrackVector;
  result->push_back(f1);
  result->push_back(f2);

  for(size_t hpw=0; hpw<result->size(); hpw++)
  {
    energyBalance-=result->operator[](hpw)->Get4Momentum().t();
    chargeBalance-=G4lrint(result->operator[](hpw)->GetDefinition()->GetPDGCharge());
    baryonBalance-=result->operator[](hpw)->GetDefinition()->GetBaryonNumber();
  }
  if(getenv("AbsorptionEnergyBalanceCheck"))
    std::cout << "DEBUGGING energy balance B: "
              <<energyBalance<<" "
	      <<chargeBalance<<" "
	      <<baryonBalance<<" "
  	      <<G4endl;
  return result;
}


G4double G4MesonAbsorption::
GetTimeToAbsorption(const G4KineticTrack& trk1, const G4KineticTrack& trk2)
{
  if(trk1.GetDefinition()!=G4PionPlus::PionPlusDefinition() &&
     trk1.GetDefinition()!=G4PionMinus::PionMinusDefinition() &&
     trk2.GetDefinition()!=G4PionPlus::PionPlusDefinition() &&
     trk2.GetDefinition()!=G4PionMinus::PionMinusDefinition())
  {
    return DBL_MAX;
  }  
  G4double time = DBL_MAX;
  G4double sqrtS = (trk1.Get4Momentum() + trk2.Get4Momentum()).mag();

  // Check whether there is enough energy for elastic scattering 
  // (to put the particles on to mass shell
  
  if (trk1.GetActualMass() + trk2.GetActualMass() < sqrtS)
  {
    G4LorentzVector mom1 = trk1.GetTrackingMomentum();      
    G4ThreeVector position = trk1.GetPosition() - trk2.GetPosition();    
    if ( mom1.mag2() < -1.*eV )
    {
      G4cout << "G4MesonAbsorption::GetTimeToInteraction(): negative m2:" << mom1.mag2() << G4endl;
    } 
    G4ThreeVector velocity = mom1.vect()/mom1.e() * c_light; 
    G4double collisionTime = - (position * velocity) / (velocity * velocity); 

    if (collisionTime > 0)
    { 
      G4LorentzVector mom2(0,0,0,trk2.Get4Momentum().mag());
      G4LorentzRotation toCMSFrame((-1)*(mom1 + mom2).boostVector());
      mom1 = toCMSFrame * mom1;
      mom2 = toCMSFrame * mom2;

      G4LorentzVector coordinate1(trk1.GetPosition(), 100.);
      G4LorentzVector coordinate2(trk2.GetPosition(), 100.);
      G4ThreeVector pos = ((toCMSFrame * coordinate1).vect() - 
			    (toCMSFrame * coordinate2).vect());
      G4ThreeVector mom = mom1.vect() - mom2.vect();

      G4double distance = pos * pos - (pos*mom) * (pos*mom) / (mom*mom);

      // global optimization
      static const G4double maxCrossSection = 500*millibarn;
      if(pi*distance>maxCrossSection) return time;	   
      // charged particles special optimization
      static const G4double maxChargedCrossSection = 200*millibarn;
      if(abs(trk1.GetDefinition()->GetPDGCharge())>0.1 && 
        abs(trk2.GetDefinition()->GetPDGCharge())>0.1 &&
	pi*distance>maxChargedCrossSection) return time;	      
      // neutrons special optimization 
      if(( trk1.GetDefinition() == G4Neutron::Neutron() ||
	   trk1.GetDefinition() == G4Neutron::Neutron() ) &&
	   sqrtS>1.91*GeV && pi*distance>maxChargedCrossSection) return time;

      G4double totalCrossSection = AbsorptionCrossSection(trk1,trk2);
      if ( totalCrossSection > 0 )
      {
	if (distance <= totalCrossSection / pi)
	{
	  time = collisionTime;
	}
      } 
    }
  }
  return time;
}

G4double G4MesonAbsorption::
AbsorptionCrossSection(const G4KineticTrack & aT, const G4KineticTrack & bT)
{
  G4double t = 0;
  if(aT.GetDefinition()==G4PionPlus::PionPlusDefinition() ||
     aT.GetDefinition()==G4PionMinus::PionMinusDefinition() )
  {
    t = aT.Get4Momentum().t()-aT.Get4Momentum().mag()/MeV;
  } 
  else if(bT.GetDefinition()==G4PionPlus::PionPlusDefinition() ||
        bT.GetDefinition()!=G4PionMinus::PionMinusDefinition())
  {
    t = bT.Get4Momentum().t()-bT.Get4Momentum().mag()/MeV;
  }

  static G4double it [26] =
        {0,4,50,5.5,75,8,95,10,120,11.5,140,12,160,11.5,180,10,190,8,210,6,235,4,260,3,300,2};

  if(t>it[24]) 
  {
    theCross = 0;
  }
  else 
  {
    G4int count = 0;
    while(t>it[count])count+=2;
    G4double x1 = it[count-2];
    G4double x2 = it[count];
    G4double y1 = it[count-1];
    G4double y2 = it[count+1];
    theCross = y1+(y2-y1)/(x2-x1)*(t-x1);
    // G4cout << "Printing the absorption crosssection "
    //        <<x1<< " "<<x2<<" "<<t<<" "<<y1<<" "<<y2<<" "<<0.5*theCross<<G4endl;
  }
  return .5*theCross*millibarn;
}