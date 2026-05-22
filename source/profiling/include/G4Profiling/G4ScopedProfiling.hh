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
  std::string category;
  // g4event
  std::int32_t eventNumber = -1;
  // g4track
  std::int32_t trackID = -1;
  std::int32_t pdgID = 0;  // PDG encoding (int); more efficient than particle name string
  // g4step
  std::int32_t stepNumber = -1;
  // g4step, g4process, g4navigation
  std::string pv;  // full physical volume path (slash-separated PV names root→leaf)
  std::string lv;  // full logical volume path  (slash-separated LV names root→leaf)
};

class G4ScopedProfiling
{
  public:
    static bool enabled();
    static G4ProfilingVerbosity verbosity();
    static void SetVerbosity(G4ProfilingVerbosity value);

    // Emit a flow-source instant event (call inside a g4process span for each secondary).
    static void EmitFlowSource(const void* id);
    // Emit a flow-sink instant event (call inside a g4track span for secondary tracks).
    static void EmitFlowSink(const void* id);

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
  const char* category = "";
  // g4event
  int eventNumber = -1;
  // g4track
  int trackID = -1;
  int pdgID = 0;
  // g4step
  int stepNumber = -1;
  // g4step, g4process, g4navigation
  const char* pv = "";  // full physical volume path (slash-separated PV names root→leaf)
  const char* lv = "";  // full logical volume path  (slash-separated LV names root→leaf)
};

class G4ScopedProfiling
{
  public:
    static bool enabled() { return false; }
    static G4ProfilingVerbosity verbosity() { return G4ProfilingVerbosity::kCoarse; }
    static void SetVerbosity(G4ProfilingVerbosity) {}
    static void EmitFlowSource(const void*) {}
    static void EmitFlowSink(const void*) {}
    explicit G4ScopedProfiling(G4ScopedProfilingInput const&) {}
    ~G4ScopedProfiling() = default;
};

#endif

#endif
