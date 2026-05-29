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
// G4ProfilingCategories.perfetto.hh
// --------------------------------------------------------------------

#pragma once

#include <perfetto.h>

namespace G4Profiling
{
namespace detail
{
inline constexpr char const* g4run_category = "g4run";
inline constexpr char const* g4event_category = "g4event";
inline constexpr char const* g4track_category = "g4track";
inline constexpr char const* g4step_category = "g4step";
inline constexpr char const* g4process_category = "g4process";
inline constexpr char const* g4navigation_category = "g4navigation";
}  // namespace detail
}  // namespace G4Profiling

PERFETTO_DEFINE_CATEGORIES(
  perfetto::Category(G4Profiling::detail::g4run_category)
    .SetDescription("Geant4 run-level spans"),
  perfetto::Category(G4Profiling::detail::g4event_category)
    .SetDescription("Geant4 event-level spans"),
  perfetto::Category(G4Profiling::detail::g4track_category)
    .SetDescription("Geant4 track-level spans"),
  perfetto::Category(G4Profiling::detail::g4step_category)
    .SetDescription("Geant4 step-level spans"),
  perfetto::Category(G4Profiling::detail::g4process_category)
    .SetDescription("Geant4 process-level spans"),
  perfetto::Category(G4Profiling::detail::g4navigation_category)
    .SetDescription("Geant4 navigation spans"));
