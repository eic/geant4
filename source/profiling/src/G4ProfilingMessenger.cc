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
// G4ProfilingMessenger.cc
// --------------------------------------------------------------------

#include "G4Profiling/G4ProfilingMessenger.hh"

#include "G4Profiling/G4ProfilingManager.hh"
#include "G4UIdirectory.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithoutParameter.hh"

G4ProfilingMessenger::G4ProfilingMessenger(G4ProfilingManager* manager)
  : manager_(manager)
{
  profilingDirectory_ = new G4UIdirectory("/profiling/");
  profilingDirectory_->SetGuidance("Geant4 profiling control commands.");

  verboseCmd_ = new G4UIcmdWithAnInteger("/profiling/verbose", this);
  verboseCmd_->SetGuidance("Set profiling verbosity (cumulative).");
  verboseCmd_->SetGuidance(" 0 : run-level and event-level spans only (kCoarse)");
  verboseCmd_->SetGuidance(" 1 : adds track-level spans with particle name/pdg (kNormal)");
  verboseCmd_->SetGuidance(" 2 : adds step-level spans with immediate volume name (kFine)");
  verboseCmd_->SetGuidance("     Note: at kFine, 10 events ≈ 150 MB; enable flushEveryNEvents for long runs.");
  verboseCmd_->SetGuidance(" 3 : adds process/navigation spans, full pv/lv paths, flow events (kVerbose)");
  verboseCmd_->SetGuidance("     Note: at kVerbose, 10 events ≈ 1.9 GB; reduce bufferSizeMB + enable drain.");
  verboseCmd_->SetParameterName("level", false);
  verboseCmd_->SetRange("level>=0 && level<=3");
  verboseCmd_->AvailableForStates(G4State_PreInit, G4State_Idle);

  perfettoDirectory_ = new G4UIdirectory("/profiling/perfetto/");
  perfettoDirectory_->SetGuidance("Perfetto backend control commands.");

  outputFileCmd_ = new G4UIcmdWithAString("/profiling/perfetto/outputFile", this);
  outputFileCmd_->SetGuidance("Set the output trace file (default: geant4.pftrace).");
  outputFileCmd_->SetGuidance("Use a .pftrace extension for compatibility with ui.perfetto.dev.");
  outputFileCmd_->SetParameterName("filename", false);
  outputFileCmd_->AvailableForStates(G4State_PreInit, G4State_Idle);

  bufferSizeCmd_ = new G4UIcmdWithAnInteger("/profiling/perfetto/bufferSizeMB", this);
  bufferSizeCmd_->SetGuidance("Set the in-memory ring buffer size in MB (default: 3072 = 3 GB).");
  bufferSizeCmd_->SetGuidance("Must be large enough to hold all events between drain cycles.");
  bufferSizeCmd_->SetGuidance("Suggested values by verbosity: kFine+drain=512, kVerbose+drain=2048.");
  bufferSizeCmd_->SetGuidance("Set before /profiling/perfetto/start.");
  bufferSizeCmd_->SetParameterName("sizeMB", false);
  bufferSizeCmd_->SetRange("sizeMB>0");
  bufferSizeCmd_->AvailableForStates(G4State_PreInit, G4State_Idle);

  flushEventsCmd_ = new G4UIcmdWithAnInteger("/profiling/perfetto/flushEveryNEvents", this);
  flushEventsCmd_->SetGuidance("Drain ring buffer to disk every N events (0 = disabled, default).");
  flushEventsCmd_->SetGuidance("Enables long simulations by recycling buffer; trace file grows incrementally.");
  flushEventsCmd_->SetGuidance("Buffer must hold N events; set bufferSizeMB accordingly:");
  flushEventsCmd_->SetGuidance("  kFine: ≈20 MB/event → bufferSizeMB = 20*N + margin");
  flushEventsCmd_->SetGuidance("  kVerbose: ≈190 MB/event → bufferSizeMB = 190*N + margin");
  flushEventsCmd_->SetGuidance("Spans crossing a drain boundary may appear incomplete (acceptable).");
  flushEventsCmd_->SetParameterName("N", false);
  flushEventsCmd_->SetRange("N>=0");
  flushEventsCmd_->AvailableForStates(G4State_PreInit, G4State_Idle);

  startCmd_ = new G4UIcmdWithoutParameter("/profiling/perfetto/start", this);
  startCmd_->SetGuidance("Start the active tracing session.");
  startCmd_->AvailableForStates(G4State_PreInit, G4State_Idle);

  stopCmd_ = new G4UIcmdWithoutParameter("/profiling/perfetto/stop", this);
  stopCmd_->SetGuidance("Stop the tracing session and flush pending events.");
  stopCmd_->AvailableForStates(G4State_PreInit, G4State_Idle);
}

G4ProfilingMessenger::~G4ProfilingMessenger()
{
  delete stopCmd_;
  delete startCmd_;
  delete flushEventsCmd_;
  delete bufferSizeCmd_;
  delete outputFileCmd_;
  delete perfettoDirectory_;
  delete verboseCmd_;
  delete profilingDirectory_;
}

void G4ProfilingMessenger::SetNewValue(G4UIcommand* command, G4String value)
{
  if (command == verboseCmd_)
  {
    auto level = verboseCmd_->GetNewIntValue(value);
    switch (level)
    {
      case 0:
        manager_->SetVerbosity(G4ProfilingVerbosity::kCoarse);
        break;
      case 1:
        manager_->SetVerbosity(G4ProfilingVerbosity::kNormal);
        break;
      case 2:
        manager_->SetVerbosity(G4ProfilingVerbosity::kFine);
        break;
      default:
        manager_->SetVerbosity(G4ProfilingVerbosity::kVerbose);
        break;
    }
  }
  else if (command == outputFileCmd_)
  {
    manager_->SetOutputFileName(value);
  }
  else if (command == bufferSizeCmd_)
  {
    manager_->SetBufferSizeMB(static_cast<std::size_t>(bufferSizeCmd_->GetNewIntValue(value)));
  }
  else if (command == flushEventsCmd_)
  {
    manager_->SetFlushEveryNEvents(flushEventsCmd_->GetNewIntValue(value));
  }
  else if (command == startCmd_)
  {
    manager_->StartTracing();
  }
  else if (command == stopCmd_)
  {
    manager_->FlushTracing();
    manager_->StopTracing();
  }
}

G4String G4ProfilingMessenger::GetCurrentValue(G4UIcommand* command)
{
  if (command == verboseCmd_)
  {
    return verboseCmd_->ConvertToString(static_cast<G4int>(manager_->GetVerbosity()));
  }
  if (command == outputFileCmd_)
  {
    return manager_->GetOutputFileName();
  }
  if (command == bufferSizeCmd_)
  {
    return bufferSizeCmd_->ConvertToString(static_cast<G4int>(manager_->GetBufferSizeMB()));
  }
  if (command == flushEventsCmd_)
  {
    return flushEventsCmd_->ConvertToString(manager_->GetFlushEveryNEvents());
  }
  return "";
}
