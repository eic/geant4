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
#include <unordered_set>

#include "detail/G4ProfilingCategories.perfetto.hh"

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

namespace
{
// Return a stable const char* for s that persists for the thread lifetime,
// allowing perfetto::StaticString (no per-event copy) instead of DynamicString.
const char* InternString(std::string const& s)
{
  thread_local std::unordered_set<std::string> pool;
  return pool.emplace(s).first->c_str();
}

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

  // Begin a perfetto track event with category-specific debug arguments.
  // display_color carries an ARGB color for the trace viewer.
  auto emitColor = static_cast<std::uint32_t>(input.color);

  if (!activated && category_ == g4run_category) {
    TRACE_EVENT_BEGIN(g4run_category,
                      perfetto::DynamicString{input.name},
                      "display_color", emitColor);
    activated = true;
  }
  if (!activated && category_ == g4event_category) {
    TRACE_EVENT_BEGIN(g4event_category,
                      perfetto::DynamicString{input.name},
                      "display_color", emitColor,
                      "event_number", static_cast<std::int32_t>(input.eventNumber));
    activated = true;
  }
  if (!activated && category_ == g4track_category) {
    TRACE_EVENT_BEGIN(g4track_category,
                      perfetto::DynamicString{input.name},
                      "display_color", emitColor,
                      "track_id",      static_cast<std::int32_t>(input.trackID),
                      "pdg_id",        static_cast<std::int32_t>(input.pdgID));
    activated = true;
  }
  if (!activated && category_ == g4step_category) {
    TRACE_EVENT_BEGIN(g4step_category,
                      perfetto::StaticString{InternString(input.name)},
                      "display_color", emitColor,
                      "step_number",   static_cast<std::int32_t>(input.stepNumber),
                      "pv",            perfetto::StaticString{InternString(input.pv)},
                      "lv",            perfetto::StaticString{InternString(input.lv)});
    activated = true;
  }
  if (!activated && category_ == g4process_category) {
    TRACE_EVENT_BEGIN(g4process_category,
                      perfetto::DynamicString{input.name},
                      "display_color", emitColor,
                      "pv",            perfetto::StaticString{InternString(input.pv)});
    activated = true;
  }
  if (!activated && category_ == g4navigation_category) {
    TRACE_EVENT_BEGIN(g4navigation_category,
                      perfetto::DynamicString{input.name},
                      "display_color", emitColor,
                      "pv",            perfetto::StaticString{InternString(input.pv)});
    activated = true;
  }

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

// Emit a flow-source instant event inside a g4process span for a secondary track.
// The pointer serves as the flow ID; use EmitFlowSink with the same pointer (the
// G4Track*) at the start of the corresponding g4track span.
void G4ScopedProfiling::EmitFlowSource(const void* id)
{
  using namespace G4Profiling::detail;
  TRACE_EVENT_INSTANT(g4process_category, perfetto::StaticString{"secondary"},
                      perfetto::Flow::FromPointer(const_cast<void*>(id)));
}

// Emit a flow-sink instant event inside a g4track span for a secondary track.
void G4ScopedProfiling::EmitFlowSink(const void* id)
{
  using namespace G4Profiling::detail;
  TRACE_EVENT_INSTANT(g4track_category, perfetto::StaticString{"from_parent"},
                      perfetto::TerminatingFlow::FromPointer(const_cast<void*>(id)));
}
