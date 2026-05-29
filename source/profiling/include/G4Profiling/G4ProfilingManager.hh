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
// G4ProfilingManager.hh
// --------------------------------------------------------------------

#ifndef G4ProfilingManager_hh
#define G4ProfilingManager_hh 1

#include "G4Profiling/G4ScopedProfiling.hh"

#include <atomic>
#include <memory>
#include <string>

class G4ProfilingMessenger;

class G4ProfilingManager
{
  public:
    static G4ProfilingManager& GetInstance();

    ~G4ProfilingManager();

    bool IsEnabled() const;
    bool IsTracingActive() const;
    G4ProfilingVerbosity GetVerbosity() const;
    void SetVerbosity(G4ProfilingVerbosity value);

    bool IsCategoryEnabled(std::string const& category) const;

    void SetOutputFileName(std::string const& filename);
    std::string GetOutputFileName() const;

    void StartTracing();
    void StopTracing();
    void FlushTracing();

    G4ProfilingManager(G4ProfilingManager const&) = delete;
    G4ProfilingManager& operator=(G4ProfilingManager const&) = delete;

  private:
    G4ProfilingManager();

  private:
    std::atomic<int> verbosity_{static_cast<int>(G4ProfilingVerbosity::kCoarse)};
    std::atomic<bool> active_{false};
    std::string outputFileName_;
    std::unique_ptr<G4ProfilingMessenger> messenger_;
};

#endif
