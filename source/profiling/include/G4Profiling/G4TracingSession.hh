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
// G4TracingSession.hh
// --------------------------------------------------------------------

#ifndef G4TracingSession_hh
#define G4TracingSession_hh 1

#include "G4Profiling/G4ProfilingConfig.hh"

#if defined(GEANT4_USE_PROFILING) && defined(GEANT4_USE_PERFETTO)

#include <memory>
#include <string>

class G4TracingSessionImpl;

class G4TracingSession
{
  public:
    static G4TracingSession& Instance();

    void Start(std::string const& filename);
    void Stop();
    void Flush();
    bool IsActive() const;

    ~G4TracingSession();

    G4TracingSession(G4TracingSession const&) = delete;
    G4TracingSession& operator=(G4TracingSession const&) = delete;

  private:
    G4TracingSession();

  private:
    std::unique_ptr<G4TracingSessionImpl> impl_;
};

#else

#include <string>

class G4TracingSession
{
  public:
    static G4TracingSession& Instance()
    {
      static G4TracingSession instance;
      return instance;
    }

    void Start(std::string const&) {}
    void Stop() {}
    void Flush() {}
    bool IsActive() const { return false; }
    ~G4TracingSession() = default;

    G4TracingSession(G4TracingSession const&) = delete;
    G4TracingSession& operator=(G4TracingSession const&) = delete;

  private:
    G4TracingSession() = default;
};

#endif

#endif
