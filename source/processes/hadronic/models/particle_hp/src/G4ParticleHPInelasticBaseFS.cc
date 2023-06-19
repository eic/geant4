//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
// particle_hp -- source file
// J.P. Wellisch, Nov-1996
// A prototype of the low energy neutron transport model.
//
// 080801 Give a warning message for irregular mass value in data file by T. Koi
//        Introduce theNDLDataA,Z which has A and Z of NDL data by T. Koi
// 081024 G4NucleiPropertiesTable:: to G4NucleiProperties::
// 101111 Add Special treatment for Be9(n,2n)Be8(2a) case by T. Koi
//
// P. Arce, June-2014 Conversion neutron_hp to particle_hp
//
// June-2019 - E. Mendoza --> Added protection against residual with Z<0 or A<Z + adjust_final_state
// is not applied when data is in MF=6 format (no correlated particle emission) + bug correction
// (add Q value info to G4ParticleHPNBodyPhaseSpace).

#include "G4ParticleHPInelasticBaseFS.hh"

#include "G4Alpha.hh"
#include "G4Electron.hh"
#include "G4He3.hh"
#include "G4IonTable.hh"
#include "G4NucleiProperties.hh"
#include "G4Nucleus.hh"
#include "G4ParticleHPDataUsed.hh"
#include "G4ParticleHPManager.hh"

void G4ParticleHPInelasticBaseFS::InitGammas(G4double AR, G4double ZR)
{
  std::ostringstream ost;
  ost << gammaPath << "z" << ZR << ".a" << AR;
  G4String aName = ost.str();
  std::ifstream from(aName, std::ios::in);

  if (!from) return;  // no data found for this isotope
  std::ifstream theGammaData(aName, std::ios::in);

  G4double eps = 0.001;
  theNuclearMassDifference =
    G4NucleiProperties::GetBindingEnergy(static_cast<G4int>(AR + eps), static_cast<G4int>(ZR + eps))
    - G4NucleiProperties::GetBindingEnergy(static_cast<G4int>(theBaseA + eps),
                                           static_cast<G4int>(theBaseZ + eps));
  theGammas.Init(theGammaData);
}

void G4ParticleHPInelasticBaseFS::Init(G4double A, G4double Z, G4int M, G4String& dirName,
                                       G4String& bit, G4ParticleDefinition*)
{
  gammaPath = "/Inelastic/Gammas/";
  if (!G4FindDataDir("G4NEUTRONHPDATA"))
    throw G4HadronicException(__FILE__, __LINE__,
                              "Please setenv G4NEUTRONHPDATA to point to the neutron cross-section "
                              "files where Inelastic/Gammas data is found.");
  G4String tBase = G4FindDataDir("G4NEUTRONHPDATA");
  gammaPath = tBase + gammaPath;
  G4String tString = dirName;
  G4bool dbool;
  G4ParticleHPDataUsed aFile =
    theNames.GetName(static_cast<G4int>(A), static_cast<G4int>(Z), M, tString, bit, dbool);
  G4String filename = aFile.GetName();
#ifdef G4PHPDEBUG
  if (std::getenv("G4ParticleHPDebug"))
    G4cout << " G4ParticleHPInelasticBaseFS::Init FILE " << filename << G4endl;
#endif
  SetAZMs(A, Z, M, aFile);

  if (!dbool
      || (Z < 2.5 && (std::abs(theNDLDataZ - Z) > 0.0001 || std::abs(theNDLDataA - A) > 0.0001)))
  {
#ifdef G4PHPDEBUG
    if (std::getenv("G4ParticleHPDebug_NamesLogging"))
      G4cout << "Skipped = " << filename << " " << A << " " << Z << G4endl;
#endif
    hasAnyData = false;
    hasFSData = false;
    hasXsec = false;
    return;
  }

  std::istringstream theData(std::ios::in);
  G4ParticleHPManager::GetInstance()->GetDataStream(filename, theData);

  if (!theData)  //"!" is a operator of ios
  {
    hasAnyData = false;
    hasFSData = false;
    hasXsec = false;
    //   theData.close();
    return;  // no data for exactly this isotope and FS
  }
  // here we go
  G4int infoType, dataType, dummy = INT_MAX;
  hasFSData = false;
  while (theData >> infoType)  // Loop checking, 11.05.2015, T. Koi
  {
    theData >> dataType;

    if (dummy == INT_MAX) theData >> Qvalue >> dummy;
    Qvalue *= CLHEP::eV;
    // In G4NDL4.5 this value is the MT number (<1000),
    // in others is que Q-value in eV

    if (dataType == 3) {
      G4int total;
      theData >> total;
      theXsection->Init(theData, total, CLHEP::eV);
    }
    else if (dataType == 4) {
      theAngularDistribution = new G4ParticleHPAngular;
      theAngularDistribution->Init(theData);
      hasFSData = true;
    }
    else if (dataType == 5) {
      theEnergyDistribution = new G4ParticleHPEnergyDistribution;
      theEnergyDistribution->Init(theData);
      hasFSData = true;
    }
    else if (dataType == 6) {
      theEnergyAngData = new G4ParticleHPEnAngCorrelation(theProjectile);
      theEnergyAngData->Init(theData);
      hasFSData = true;
    }
    else if (dataType == 12) {
      theFinalStatePhotons = new G4ParticleHPPhotonDist;
      theFinalStatePhotons->InitMean(theData);
      hasFSData = true;
    }
    else if (dataType == 13) {
      theFinalStatePhotons = new G4ParticleHPPhotonDist;
      theFinalStatePhotons->InitPartials(theData, theXsection);
      hasFSData = true;
    }
    else if (dataType == 14) {
      theFinalStatePhotons->InitAngular(theData);
      hasFSData = true;
    }
    else if (dataType == 15) {
      theFinalStatePhotons->InitEnergies(theData);
      hasFSData = true;
    }
    else {
      throw G4HadronicException(__FILE__, __LINE__,
                                "Data-type unknown to G4ParticleHPInelasticBaseFS");
    }
  }
}

void G4ParticleHPInelasticBaseFS::BaseApply(const G4HadProjectile& theTrack,
                                            G4ParticleDefinition** theDefs, G4int nDef)
{
  // prepare neutron
  if (theResult.Get() == NULL) theResult.Put(new G4HadFinalState);
  theResult.Get()->Clear();
  G4double eKinetic = theTrack.GetKineticEnergy();
  const G4HadProjectile* hadProjectile = &theTrack;
  G4ReactionProduct incidReactionProduct(
    const_cast<G4ParticleDefinition*>(hadProjectile->GetDefinition()));
  incidReactionProduct.SetMomentum(hadProjectile->Get4Momentum().vect());
  incidReactionProduct.SetKineticEnergy(eKinetic);

  // prepare target
  G4double targetMass;
  G4double eps = 0.0001;
  targetMass = (G4NucleiProperties::GetNuclearMass(static_cast<G4int>(theBaseA + eps),
                                                   static_cast<G4int>(theBaseZ + eps)))
               /
               // theProjectile->GetPDGMass();
               G4Neutron::Neutron()->GetPDGMass();

  // give priority to ENDF vales for target mass
  if (theEnergyAngData != 0) {
    targetMass = theEnergyAngData->GetTargetMass();
  }
  if (theAngularDistribution != 0) {
    targetMass = theAngularDistribution->GetTargetMass();
  }

  // 110512 TKDB ENDF-VII.0 21Sc45 has trouble in MF4MT22 (n,np) targetMass is not properly
  // recorded.
  if (targetMass == 0) {
    // G4cout << "TKDB targetMass = 0; ENDF-VII.0 21Sc45 has trouble in MF4MT22 (n,np) targetMass is
    // not properly recorded. This could be a similar situation." << G4endl; targetMass = (
    // G4NucleiProperties::GetNuclearMass(static_cast<G4int>(theBaseA+eps),
    // static_cast<G4int>(theBaseZ+eps))) / theProjectile->GetPDGMass();
    targetMass = (G4NucleiProperties::GetNuclearMass(static_cast<G4int>(theBaseA + eps),
                                                     static_cast<G4int>(theBaseZ + eps)))
                 / G4Neutron::Neutron()->GetPDGMass();
  }

  G4Nucleus aNucleus;
  G4ReactionProduct theTarget;

  G4ThreeVector neuVelo =
    (1. / G4Neutron::Neutron()->GetPDGMass()) * incidReactionProduct.GetMomentum();
  theTarget =
    aNucleus.GetBiasedThermalNucleus(targetMass, neuVelo, theTrack.GetMaterial()->GetTemperature());

  theTarget.SetDefinition(G4IonTable::GetIonTable()->GetIon(G4int(theBaseZ), G4int(theBaseA), 0.0));

  // prepare energy in target rest frame
  G4ReactionProduct boosted;
  boosted.Lorentz(incidReactionProduct, theTarget);
  eKinetic = boosted.GetKineticEnergy();
  G4double orgMomentum = boosted.GetMomentum().mag();

  // Take N-body phase-space distribution, if no other data present.
  if (!HasFSData())  // adding the residual is trivial here @@@
  {
    G4ParticleHPNBodyPhaseSpace thePhaseSpaceDistribution;
    G4double aPhaseMass = 0;
    G4int ii;
    for (ii = 0; ii < nDef; ++ii) {
      aPhaseMass += theDefs[ii]->GetPDGMass();
    }

    //----------------------------------------------------------------------------
    if (Qvalue < 1. * CLHEP::keV && Qvalue > -1. * CLHEP::keV) {
      // Not in the G4NDL lib or not calculated yet:

      // Calculate residual:
      G4int ResidualA = theBaseA;
      G4int ResidualZ = theBaseZ;
      for (ii = 0; ii < nDef; ++ii) {
        ResidualZ -= theDefs[ii]->GetAtomicNumber();
        ResidualA -= theDefs[ii]->GetBaryonNumber();
      }

      if (ResidualA > 0 && ResidualZ > 0) {
        G4ParticleDefinition* resid = G4IonTable::GetIonTable()->GetIon(ResidualZ, ResidualA);
        Qvalue =
          incidReactionProduct.GetMass() + theTarget.GetMass() - aPhaseMass - resid->GetPDGMass();
      }

      if (Qvalue > 400 * CLHEP::MeV || Qvalue < -400 * CLHEP::MeV) {
        // Then Q value is probably too large ...
        Qvalue = 1.1 * CLHEP::keV;
      }
    }
    //----------------------------------------------------------------------------

    thePhaseSpaceDistribution.Init(aPhaseMass, nDef);
    thePhaseSpaceDistribution.SetProjectileRP(&incidReactionProduct);
    thePhaseSpaceDistribution.SetTarget(&theTarget);
    thePhaseSpaceDistribution.SetQValue(Qvalue);

    for (ii = 0; ii < nDef; ++ii) {
      G4double massCode = 1000. * std::abs(theDefs[ii]->GetPDGCharge());
      massCode += theDefs[ii]->GetBaryonNumber();
      G4double dummy = 0;
      G4ReactionProduct* aSec = thePhaseSpaceDistribution.Sample(eKinetic, massCode, dummy);
      aSec->Lorentz(*aSec, -1. * theTarget);
      G4DynamicParticle* aPart = new G4DynamicParticle();
      aPart->SetDefinition(aSec->GetDefinition());
      aPart->SetMomentum(aSec->GetMomentum());
      delete aSec;
      theResult.Get()->AddSecondary(aPart, secID);
#ifdef G4PHPDEBUG
      if (std::getenv("G4ParticleHPDebug"))
        G4cout << this << " G4ParticleHPInelasticBaseFS::BaseApply NoFSData add secondary "
               << aPart->GetParticleDefinition()->GetParticleName()
               << " E= " << aPart->GetKineticEnergy() << " NSECO "
               << theResult.Get()->GetNumberOfSecondaries() << G4endl;
#endif
    }
    theResult.Get()->SetStatusChange(stopAndKill);
    // Final momentum check should be done before return
    G4ParticleDefinition* targ_pd =
      G4IonTable::GetIonTable()->GetIon((G4int)theBaseZ, (G4int)theBaseA, 0.0);
    G4LorentzVector targ_4p_lab(
      theTarget.GetMomentum(),
      std::sqrt(targ_pd->GetPDGMass() * targ_pd->GetPDGMass() + theTarget.GetMomentum().mag2()));
    G4LorentzVector proj_4p_lab = theTrack.Get4Momentum();
    G4LorentzVector init_4p_lab = proj_4p_lab + targ_4p_lab;
    adjust_final_state(init_4p_lab);

    return;
  }

  // set target and neutron in the relevant exit channel
  if (theAngularDistribution != 0) {
    theAngularDistribution->SetTarget(theTarget);
    theAngularDistribution->SetProjectileRP(incidReactionProduct);
  }
  else if (theEnergyAngData != 0) {
    theEnergyAngData->SetTarget(theTarget);
    theEnergyAngData->SetProjectileRP(incidReactionProduct);
  }

  G4ReactionProductVector* tmpHadrons = 0;
#ifdef G4PHPDEBUG
  // To avoid compilation error around line 532.
  G4int ii(0);
#endif
  G4int dummy;
  std::size_t i;

  if (theEnergyAngData != 0) {
    tmpHadrons = theEnergyAngData->Sample(eKinetic);

    if (!G4ParticleHPManager::GetInstance()->GetDoNotAdjustFinalState()) {
      // Adjust A and Z in the case of miss much between selected data and target nucleus
      if (tmpHadrons != nullptr) {
        G4int sumA = 0;
        G4int sumZ = 0;
        G4int maxA = 0;
        G4int jAtMaxA = 0;
        for (G4int j = 0; j != (G4int)tmpHadrons->size(); ++j) {
          // G4cout << __FILE__ << " " << __LINE__ << "th line:
          // tmpHadrons->at(j)->GetDefinition()->GetParticleName() = " <<
          // tmpHadrons->at(j)->GetDefinition()->GetParticleName() << G4endl;
          if (tmpHadrons->at(j)->GetDefinition()->GetBaryonNumber() > maxA) {
            maxA = tmpHadrons->at(j)->GetDefinition()->GetBaryonNumber();
            jAtMaxA = j;
          }
          sumA += tmpHadrons->at(j)->GetDefinition()->GetBaryonNumber();
          sumZ += G4int(tmpHadrons->at(j)->GetDefinition()->GetPDGCharge() + eps);
        }
        G4int dA = (G4int)theBaseA + hadProjectile->GetDefinition()->GetBaryonNumber() - sumA;
        G4int dZ =
          (G4int)theBaseZ + G4int(hadProjectile->GetDefinition()->GetPDGCharge() + eps) - sumZ;
        if (dA < 0 || dZ < 0) {
          G4int newA = tmpHadrons->at(jAtMaxA)->GetDefinition()->GetBaryonNumber() + dA;
          G4int newZ = G4int(tmpHadrons->at(jAtMaxA)->GetDefinition()->GetPDGCharge() + eps) + dZ;
          if (newA > newZ && newZ > 0) {
            G4ParticleDefinition* pd = G4IonTable::GetIonTable()->GetIon(newZ, newA);
            tmpHadrons->at(jAtMaxA)->SetDefinition(pd);
          }
        }
      }
    }
  }
  else if (theAngularDistribution != 0) {
    G4bool* Done = new G4bool[nDef];
    G4int i0;
    for (i0 = 0; i0 < nDef; ++i0)
      Done[i0] = false;

    tmpHadrons = new G4ReactionProductVector;
    G4ReactionProduct* aHadron;
    G4double localMass = (G4NucleiProperties::GetNuclearMass(static_cast<G4int>(theBaseA + eps),
                                                             static_cast<G4int>(theBaseZ + eps)));
    G4ThreeVector bufferedDirection(0, 0, 0);
    for (i0 = 0; i0 < nDef; ++i0) {
      if (!Done[i0]) {
        aHadron = new G4ReactionProduct;
        if (theEnergyDistribution != 0) {
          aHadron->SetDefinition(theDefs[i0]);
          aHadron->SetKineticEnergy(theEnergyDistribution->Sample(eKinetic, dummy));
        }
        else if (nDef == 1) {
          aHadron->SetDefinition(theDefs[i0]);
          aHadron->SetKineticEnergy(eKinetic);
        }
        else if (nDef == 2) {
          aHadron->SetDefinition(theDefs[i0]);
          aHadron->SetKineticEnergy(50 * CLHEP::MeV);
        }
        else {
          throw G4HadronicException(
            __FILE__, __LINE__,
            "No energy distribution to sample from in InelasticBaseFS::BaseApply");
        }
        theAngularDistribution->SampleAndUpdate(*aHadron);
        if (theEnergyDistribution == 0 && nDef == 2) {
          if (i0 == 0) {
            G4double mass1 = theDefs[0]->GetPDGMass();
            G4double mass2 = theDefs[1]->GetPDGMass();
            G4double massn = theProjectile->GetPDGMass();
            G4int z1 = static_cast<G4int>(theBaseZ + eps - theDefs[0]->GetPDGCharge()
                                          - theDefs[1]->GetPDGCharge());
            G4int a1 = static_cast<G4int>(theBaseA + eps) - theDefs[0]->GetBaryonNumber()
                       - theDefs[1]->GetBaryonNumber();
            G4double concreteMass = G4NucleiProperties::GetNuclearMass(a1, z1);
            G4double availableEnergy = eKinetic + massn + localMass - mass1 - mass2 - concreteMass;
            // available kinetic energy in CMS (non relativistic)
            G4double emin =
              availableEnergy + mass1 + mass2
              - std::sqrt((mass1 + mass2) * (mass1 + mass2) + orgMomentum * orgMomentum);
            G4double p1 = std::sqrt(2. * mass2 * emin);
            bufferedDirection = p1 * aHadron->GetMomentum().unit();
#ifdef G4PHPDEBUG
            if (std::getenv("G4ParticleHPDebug"))  // @@@@@ verify the nucleon counting...
            {
              G4cout << "G4ParticleHPInelasticBaseFS " << z1 << " " << theBaseZ << " " << a1 << " "
                     << theBaseA << " " << availableEnergy << " " << emin << G4endl;
            }
#endif
          }
          else {
            bufferedDirection = -bufferedDirection;
          }
          // boost from cms to lab
#ifdef G4PHPDEBUG
          if (std::getenv("G4ParticleHPDebug")) {
            G4cout << " G4ParticleHPInelasticBaseFS " << bufferedDirection.mag2() << G4endl;
          }
#endif
          aHadron->SetTotalEnergy(
            std::sqrt(aHadron->GetMass() * aHadron->GetMass() + bufferedDirection.mag2()));
          aHadron->SetMomentum(bufferedDirection);
          aHadron->Lorentz(*aHadron, -1. * (theTarget + incidReactionProduct));
#ifdef G4PHPDEBUG
          if (std::getenv("G4ParticleHPDebug")) {
            G4cout << " G4ParticleHPInelasticBaseFS " << aHadron->GetTotalEnergy() << " "
                   << aHadron->GetMomentum() << G4endl;
          }
#endif
        }
        tmpHadrons->push_back(aHadron);
#ifdef G4PHPDEBUG
        if (std::getenv("G4ParticleHPDebug"))
          G4cout << " G4ParticleHPInelasticBaseFS::BaseApply FSData add secondary "
                 << aHadron->GetDefinition()->GetParticleName()
                 << " E= " << aHadron->GetKineticEnergy() << G4endl;
#endif
      }
    }
    delete[] Done;
  }
  else {
    throw G4HadronicException(__FILE__, __LINE__, "No data to create the neutrons in NInelasticFS");
  }

  G4ReactionProductVector* thePhotons = nullptr;
  if (theFinalStatePhotons != 0) {
    // the photon distributions are in the Nucleus rest frame.
    G4ReactionProduct boosted_tmp;
    boosted_tmp.Lorentz(incidReactionProduct, theTarget);
    G4double anEnergy = boosted_tmp.GetKineticEnergy();
    thePhotons = theFinalStatePhotons->GetPhotons(anEnergy);
    if (thePhotons != 0) {
      for (i = 0; i < thePhotons->size(); ++i) {
        // back to lab
        thePhotons->operator[](i)->Lorentz(*(thePhotons->operator[](i)), -1. * theTarget);
      }
    }
  }
  else if (theEnergyAngData != 0) {
    // PA130927: do not create photons to adjust binding energy
    G4bool bAdjustPhotons = true;
#ifdef PHP_AS_HP
    bAdjustPhotons = true;
#else
    if (G4ParticleHPManager::GetInstance()->GetDoNotAdjustFinalState()) bAdjustPhotons = false;
#endif

    if (bAdjustPhotons) {
      G4double theGammaEnergy = theEnergyAngData->GetTotalMeanEnergy();
      G4double anEnergy = boosted.GetKineticEnergy();
      theGammaEnergy = anEnergy - theGammaEnergy;
      theGammaEnergy += theNuclearMassDifference;
      G4double eBindProducts = 0;
      G4double eBindN = 0;
      G4double eBindP = 0;
      G4double eBindD = G4NucleiProperties::GetBindingEnergy(2, 1);
      G4double eBindT = G4NucleiProperties::GetBindingEnergy(3, 1);
      G4double eBindHe3 = G4NucleiProperties::GetBindingEnergy(3, 2);
      G4double eBindA = G4NucleiProperties::GetBindingEnergy(4, 2);
      G4int ia = 0;
      for (i = 0; i < tmpHadrons->size(); i++) {
        if (tmpHadrons->operator[](i)->GetDefinition() == G4Neutron::Neutron()) {
          eBindProducts += eBindN;
        }
        else if (tmpHadrons->operator[](i)->GetDefinition() == G4Proton::Proton()) {
          eBindProducts += eBindP;
        }
        else if (tmpHadrons->operator[](i)->GetDefinition() == G4Deuteron::Deuteron()) {
          eBindProducts += eBindD;
        }
        else if (tmpHadrons->operator[](i)->GetDefinition() == G4Triton::Triton()) {
          eBindProducts += eBindT;
        }
        else if (tmpHadrons->operator[](i)->GetDefinition() == G4He3::He3()) {
          eBindProducts += eBindHe3;
        }
        else if (tmpHadrons->operator[](i)->GetDefinition() == G4Alpha::Alpha()) {
          eBindProducts += eBindA;
          ia++;
        }
      }

      theGammaEnergy += eBindProducts;

#ifdef G4PHPDEBUG
      if (std::getenv("G4ParticleHPDebug"))
        G4cout << " G4ParticleHPInelasticBaseFS::BaseApply gamma Energy " << theGammaEnergy
               << " eBindProducts " << eBindProducts << G4endl;
#endif

      // Special treatment for Be9 + n -> 2n + Be8 -> 2n + a + a
      if ((G4int)(theBaseZ + eps) == 4 && (G4int)(theBaseA + eps) == 9) {
        // This only valid for G4NDL3.13,,,
        if (std::abs(theNuclearMassDifference
                     - (G4NucleiProperties::GetBindingEnergy(8, 4)
                        - G4NucleiProperties::GetBindingEnergy(9, 4)))
              < 1 * CLHEP::keV
            && ia == 2)
        {
          theGammaEnergy -= (2 * eBindA);
        }
      }

      if (theGammaEnergy > 0.0) {
	for (G4int iLevel = theGammas.GetNumberOfLevels() - 1; iLevel > 0; --iLevel) {
          G4double e = theGammas.GetLevelEnergy(iLevel);
          if (e < theGammaEnergy) {
            thePhotons = theGammas.GetDecayGammas(iLevel);
            theGammaEnergy -= e;
	    break;
          }
        }
      }
    }
  }
  // fill the result
  std::size_t nSecondaries = tmpHadrons->size();
  std::size_t nPhotons = 0;
  if (thePhotons != 0) {
    nPhotons = thePhotons->size();
  }
  nSecondaries += nPhotons;
  G4DynamicParticle* theSec;
#ifdef G4PHPDEBUG
  if (std::getenv("G4ParticleHPDebug"))
    G4cout << " G4ParticleHPInelasticBaseFS::BaseApply N hadrons " << nSecondaries - nPhotons
           << G4endl;
#endif

  for (i = 0; i < nSecondaries - nPhotons; ++i) {
    theSec = new G4DynamicParticle;
    theSec->SetDefinition(tmpHadrons->operator[](i)->GetDefinition());
    theSec->SetMomentum(tmpHadrons->operator[](i)->GetMomentum());
    theResult.Get()->AddSecondary(theSec, secID);
#ifdef G4PHPDEBUG
    if (std::getenv("G4ParticleHPDebug"))
      G4cout << this << " G4ParticleHPInelasticBaseFS::BaseApply add secondary2 "
             << theSec->GetParticleDefinition()->GetParticleName()
             << " E= " << theSec->GetKineticEnergy() << " NSECO "
             << theResult.Get()->GetNumberOfSecondaries() << G4endl;
#endif
    delete tmpHadrons->operator[](i);
  }
#ifdef G4PHPDEBUG
  if (std::getenv("G4ParticleHPDebug"))
    G4cout << " G4ParticleHPInelasticBaseFS::BaseApply N photons " << nPhotons << G4endl;
#endif
  if (thePhotons != 0) {
    for (i = 0; i < nPhotons; ++i) {
      theSec = new G4DynamicParticle;
      theSec->SetDefinition(thePhotons->operator[](i)->GetDefinition());
      theSec->SetMomentum(thePhotons->operator[](i)->GetMomentum());
      theResult.Get()->AddSecondary(theSec, secID);
#ifdef G4PHPDEBUG
      if (std::getenv("G4ParticleHPDebug"))
        G4cout << this << " G4ParticleHPInelasticBaseFS::BaseApply add secondary3 "
               << theSec->GetParticleDefinition()->GetParticleName()
               << " E= " << theSec->GetKineticEnergy() << " NSECO "
               << theResult.Get()->GetNumberOfSecondaries() << G4endl;
#endif
      delete thePhotons->operator[](i);
    }
  }

  // some garbage collection
  delete thePhotons;
  delete tmpHadrons;

  G4ParticleDefinition* targ_pd =
    G4IonTable::GetIonTable()->GetIon((G4int)theBaseZ, (G4int)theBaseA, 0.0);
  G4LorentzVector targ_4p_lab(
    theTarget.GetMomentum(),
    std::sqrt(targ_pd->GetPDGMass() * targ_pd->GetPDGMass() + theTarget.GetMomentum().mag2()));
  G4LorentzVector proj_4p_lab = theTrack.Get4Momentum();
  G4LorentzVector init_4p_lab = proj_4p_lab + targ_4p_lab;

  // if data in MF=6 format (no correlated particle emission), then  adjust_final_state can give
  // severe errors:
  if (theEnergyAngData == 0) {
    adjust_final_state(init_4p_lab);
  }

  // clean up the primary neutron
  theResult.Get()->SetStatusChange(stopAndKill);
}
