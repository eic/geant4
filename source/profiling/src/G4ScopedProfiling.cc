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
// G4ScopedProfiling.cc
// --------------------------------------------------------------------

#include "G4Profiling/G4ScopedProfiling.hh"

#include "G4Profiling/G4ProfilingManager.hh"

#include <perfetto.h>

#include "detail/G4ProfilingCategories.perfetto.hh"

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

namespace
{
char const* GetPerfettoCategory(std::string const& category)
{
  using namespace G4Profiling::detail;

  if (category == g4run_category) return g4run_category;
  if (category == g4event_category) return g4event_category;
  if (category == g4track_category) return g4track_category;
  if (category == g4step_category) return g4step_category;
  if (category == g4process_category) return g4process_category;
  if (category == g4navigation_category) return g4navigation_category;
  return nullptr;
}
}

bool G4ScopedProfiling::enabled()
{
  return G4ProfilingManager::GetInstance().IsEnabled();
}

G4ProfilingVerbosity G4ScopedProfiling::verbosity()
{
  return G4ProfilingManager::GetInstance().GetVerbosity();
}

void G4ScopedProfiling::SetVerbosity(G4ProfilingVerbosity value)
{
  G4ProfilingManager::GetInstance().SetVerbosity(value);
}

bool G4ScopedProfiling::Activate(G4ScopedProfilingInput const& input)
{
  auto& manager = G4ProfilingManager::GetInstance();
  if (!manager.IsEnabled() || !manager.IsCategoryEnabled(input.category))
  {
    return false;
  }

  category_ = GetPerfettoCategory(input.category);
  if (category_ == nullptr)
  {
    return false;
  }

  using namespace G4Profiling::detail;
  bool activated = false;

  // Begin a perfetto track event for the matching category.
  // The "track_id" argument carries event-specific payload (e.g., event number,
  // track ID) and "display_color" carries an ARGB color for the trace viewer.
#define G4_TRACE_EVENT_BEGIN_IF(category_name)                                 \
  if (!activated && category_ == category_name)                                \
  {                                                                            \
    TRACE_EVENT_BEGIN(category_name,                                           \
                      perfetto::DynamicString{input.name},                     \
                      "track_id",                                              \
                      static_cast<std::uint64_t>(input.payload),               \
                      "display_color",                                         \
                      static_cast<std::uint32_t>(input.color));                \
    activated = true;                                                          \
  }

  G4_TRACE_EVENT_BEGIN_IF(g4run_category)
  G4_TRACE_EVENT_BEGIN_IF(g4event_category)
  G4_TRACE_EVENT_BEGIN_IF(g4track_category)
  G4_TRACE_EVENT_BEGIN_IF(g4step_category)
  G4_TRACE_EVENT_BEGIN_IF(g4process_category)
  G4_TRACE_EVENT_BEGIN_IF(g4navigation_category)

#undef G4_TRACE_EVENT_BEGIN_IF

  return activated;
}

void G4ScopedProfiling::Deactivate() noexcept
{
  using namespace G4Profiling::detail;

  // End the perfetto track event for the stored category pointer.
  if (category_ == g4run_category)
  {
    TRACE_EVENT_END(g4run_category);
  }
  else if (category_ == g4event_category)
  {
    TRACE_EVENT_END(g4event_category);
  }
  else if (category_ == g4track_category)
  {
    TRACE_EVENT_END(g4track_category);
  }
  else if (category_ == g4step_category)
  {
    TRACE_EVENT_END(g4step_category);
  }
  else if (category_ == g4process_category)
  {
    TRACE_EVENT_END(g4process_category);
  }
  else if (category_ == g4navigation_category)
  {
    TRACE_EVENT_END(g4navigation_category);
  }
}
