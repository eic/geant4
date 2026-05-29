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
// G4ScopedProfiling.hh
//
// Class description:
//
// RAII profiling scope for Geant4 lifecycle spans.
// --------------------------------------------------------------------

#ifndef G4ScopedProfiling_hh
#define G4ScopedProfiling_hh 1

#include "G4Profiling/G4ProfilingConfig.hh"

#if defined(GEANT4_USE_PROFILING)

#include <cstdint>
#include <string>

enum class G4ProfilingVerbosity
{
  kCoarse,
  kNormal,
  kFine,
  kVerbose
};

struct G4ScopedProfilingInput
{
  std::string name;
  std::uint32_t color = 0;
  std::uint64_t payload = 0;
  std::string category;
};

class G4ScopedProfiling
{
  public:
    static bool enabled();
    static G4ProfilingVerbosity verbosity();
    static void SetVerbosity(G4ProfilingVerbosity value);

    explicit G4ScopedProfiling(G4ScopedProfilingInput const& input)
      : activated_{false}
    {
      activated_ = this->Activate(input);
    }

    ~G4ScopedProfiling()
    {
      if (activated_)
      {
        this->Deactivate();
      }
    }

    G4ScopedProfiling(G4ScopedProfiling const&) = delete;
    G4ScopedProfiling& operator=(G4ScopedProfiling const&) = delete;

  private:
    bool Activate(G4ScopedProfilingInput const& input);
    void Deactivate() noexcept;

  private:
    bool activated_;
    char const* category_ = nullptr;
};

#else

enum class G4ProfilingVerbosity
{
  kCoarse,
  kNormal,
  kFine,
  kVerbose
};

struct G4ScopedProfilingInput
{
  const char* name = "";
  unsigned int color = 0;
  unsigned long long payload = 0;
  const char* category = "";
};

class G4ScopedProfiling
{
  public:
    static bool enabled() { return false; }
    static G4ProfilingVerbosity verbosity() { return G4ProfilingVerbosity::kCoarse; }
    static void SetVerbosity(G4ProfilingVerbosity) {}
    explicit G4ScopedProfiling(G4ScopedProfilingInput const&) {}
    ~G4ScopedProfiling() = default;
};

#endif

#endif
