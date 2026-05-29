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
// G4ProfilingManager.cc
// --------------------------------------------------------------------

#include "G4Profiling/G4ProfilingManager.hh"

#include "G4AutoLock.hh"
#include "G4Profiling/G4ProfilingMessenger.hh"
#include "G4Profiling/G4TracingSession.hh"

namespace
{
G4Mutex g4ProfilingManagerMutex = G4MUTEX_INITIALIZER;
}

G4ProfilingManager& G4ProfilingManager::GetInstance()
{
  static G4ProfilingManager manager;
  return manager;
}

G4ProfilingManager::G4ProfilingManager()
  : outputFileName_("geant4.pftrace")
  , messenger_(std::make_unique<G4ProfilingMessenger>(this))
{}

G4ProfilingManager::~G4ProfilingManager() = default;

bool G4ProfilingManager::IsEnabled() const
{
  return active_.load(std::memory_order_relaxed);
}

bool G4ProfilingManager::IsTracingActive() const
{
  return this->IsEnabled();
}

G4ProfilingVerbosity G4ProfilingManager::GetVerbosity() const
{
  return static_cast<G4ProfilingVerbosity>(verbosity_.load(std::memory_order_relaxed));
}

void G4ProfilingManager::SetVerbosity(G4ProfilingVerbosity value)
{
  G4AutoLock lock(&g4ProfilingManagerMutex);
  verbosity_.store(static_cast<int>(value), std::memory_order_relaxed);
}

bool G4ProfilingManager::IsCategoryEnabled(std::string const& category) const
{
  auto const level = this->GetVerbosity();

  if (category == "g4run" || category == "g4event")
  {
    return true;
  }
  if (category == "g4track")
  {
    return level >= G4ProfilingVerbosity::kNormal;
  }
  if (category == "g4step")
  {
    return level >= G4ProfilingVerbosity::kFine;
  }
  if (category == "g4process" || category == "g4navigation")
  {
    return level >= G4ProfilingVerbosity::kVerbose;
  }
  return false;
}

void G4ProfilingManager::SetOutputFileName(std::string const& filename)
{
  G4AutoLock lock(&g4ProfilingManagerMutex);
  outputFileName_ = filename;
}

std::string G4ProfilingManager::GetOutputFileName() const
{
  G4AutoLock lock(&g4ProfilingManagerMutex);
  return outputFileName_;
}

void G4ProfilingManager::StartTracing()
{
  auto filename = this->GetOutputFileName();
  G4TracingSession::Instance().Start(filename);

  G4AutoLock lock(&g4ProfilingManagerMutex);
  active_.store(G4TracingSession::Instance().IsActive(), std::memory_order_relaxed);
}

void G4ProfilingManager::StopTracing()
{
  G4TracingSession::Instance().Stop();

  G4AutoLock lock(&g4ProfilingManagerMutex);
  active_.store(false, std::memory_order_relaxed);
}

void G4ProfilingManager::FlushTracing()
{
  G4TracingSession::Instance().Flush();
}
