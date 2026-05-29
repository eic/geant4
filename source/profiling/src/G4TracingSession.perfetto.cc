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
// G4TracingSession.perfetto.cc
// --------------------------------------------------------------------

#include "G4Profiling/G4TracingSession.hh"

#include "G4AutoLock.hh"

#include <perfetto.h>
#include <mutex>

#if defined(_WIN32)
#  include <fcntl.h>
#  include <io.h>
#  include <sys/stat.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif

#include "detail/G4ProfilingCategories.perfetto.hh"

namespace
{
G4Mutex g4TracingSessionMutex = G4MUTEX_INITIALIZER;
std::once_flag g4PerfettoInitOnce;

perfetto::TraceConfig ConfigureSession()
{
  perfetto::protos::gen::TrackEventConfig trackEventConfig;
  trackEventConfig.add_disabled_categories("*");
  trackEventConfig.add_enabled_categories(G4Profiling::detail::g4run_category);
  trackEventConfig.add_enabled_categories(G4Profiling::detail::g4event_category);
  trackEventConfig.add_enabled_categories(G4Profiling::detail::g4track_category);
  trackEventConfig.add_enabled_categories(G4Profiling::detail::g4step_category);
  trackEventConfig.add_enabled_categories(G4Profiling::detail::g4process_category);
  trackEventConfig.add_enabled_categories(G4Profiling::detail::g4navigation_category);

  perfetto::TraceConfig config;
  // 8 MB ring buffer is a reasonable default for moderate-length runs.
  // Increase for long runs or high-verbosity profiles to avoid buffer wrap.
  config.add_buffers()->set_size_kb(8 * 1024);
  auto* dataSource = config.add_data_sources()->mutable_config();
  dataSource->set_name("track_event");
  dataSource->set_track_event_config_raw(trackEventConfig.SerializeAsString());
  return config;
}

void InitializePerfetto()
{
  perfetto::TracingInitArgs args;
  args.backends |= perfetto::kInProcessBackend;
  perfetto::Tracing::Initialize(args);
  perfetto::TrackEvent::Register();
}

int OpenTraceFile(std::string const& filename)
{
#if defined(_WIN32)
  return _open(filename.c_str(), _O_BINARY | _O_RDWR | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
#else
  return open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
#endif
}

void CloseTraceFile(int fd)
{
#if defined(_WIN32)
  _close(fd);
#else
  close(fd);
#endif
}
}

class G4TracingSessionImpl
{
  public:
    static constexpr int kInvalidFd = -1;

    int fd = kInvalidFd;
    std::unique_ptr<perfetto::TracingSession> session;
};

G4TracingSession& G4TracingSession::Instance()
{
  static G4TracingSession instance;
  return instance;
}

G4TracingSession::G4TracingSession() = default;

G4TracingSession::~G4TracingSession()
{
  this->Stop();
}

void G4TracingSession::Start(std::string const& filename)
{
  G4AutoLock lock(&g4TracingSessionMutex);

  if (impl_ && impl_->session)
  {
    perfetto::TrackEvent::Flush();
    impl_->session->StopBlocking();
    if (impl_->fd != G4TracingSessionImpl::kInvalidFd)
    {
      CloseTraceFile(impl_->fd);
    }
    impl_.reset();
  }

  std::call_once(g4PerfettoInitOnce, InitializePerfetto);

  impl_ = std::make_unique<G4TracingSessionImpl>();
  impl_->fd = OpenTraceFile(filename);
  if (impl_->fd < 0)
  {
    impl_.reset();
    return;
  }

  auto session = perfetto::Tracing::NewTrace();
  if (!session)
  {
    CloseTraceFile(impl_->fd);
    impl_.reset();
    return;
  }

  session->Setup(ConfigureSession(), impl_->fd);
  session->StartBlocking();
  impl_->session = std::move(session);
}

void G4TracingSession::Stop()
{
  G4AutoLock lock(&g4TracingSessionMutex);

  if (!impl_ || !impl_->session)
  {
    return;
  }

  perfetto::TrackEvent::Flush();
  impl_->session->StopBlocking();

  if (impl_->fd != G4TracingSessionImpl::kInvalidFd)
  {
    CloseTraceFile(impl_->fd);
  }

  impl_.reset();
}

void G4TracingSession::Flush()
{
  G4AutoLock lock(&g4TracingSessionMutex);

  if (impl_ && impl_->session)
  {
    perfetto::TrackEvent::Flush();
  }
}

bool G4TracingSession::IsActive() const
{
  return impl_ && impl_->session;
}
