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

perfetto::TraceConfig ConfigureSession(std::size_t bufferSizeMB)
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
  // Use a large in-memory ring buffer and collect via ReadTraceBlocking()
  // after StopBlocking(). This avoids the write_into_file streaming path where
  // partial chunks from parked worker threads were being lost.
  // Note: with string interning and step-level tracing, 10 events ≈ 1.8 GB.
  // Use /profiling/perfetto/bufferSizeMB to tune; enable periodic draining via
  // /profiling/perfetto/flushEveryNEvents to support runs over more events.
  config.add_buffers()->set_size_kb(bufferSizeMB * 1024);
  // Disable periodic incremental-state clears to keep all interned strings
  // valid for the whole session (avoids trace-size blowup on FlushBlocking).
  config.mutable_incremental_state_config()->set_clear_period_ms(0);
  auto* dataSource = config.add_data_sources()->mutable_config();
  dataSource->set_name("track_event");
  dataSource->set_track_event_config_raw(trackEventConfig.SerializeAsString());
  return config;
}

void InitializePerfetto()
{
  perfetto::TracingInitArgs args;
  args.backends |= perfetto::kInProcessBackend;
  // Increase the shared memory buffer per producer to reduce packet loss
  // during high-frequency step-level tracing.
  args.shmem_size_hint_kb = 32 * 1024;
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
    std::size_t bufferSizeMB = 3072;
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

void G4TracingSession::Start(std::string const& filename, std::size_t bufferSizeMB)
{
  G4AutoLock lock(&g4TracingSessionMutex);

  if (impl_ && impl_->session)
  {
    impl_->session->StopBlocking();
    if (impl_->fd != G4TracingSessionImpl::kInvalidFd)
    {
      CloseTraceFile(impl_->fd);
    }
    impl_.reset();
  }

  std::call_once(g4PerfettoInitOnce, InitializePerfetto);

  impl_ = std::make_unique<G4TracingSessionImpl>();
  impl_->bufferSizeMB = bufferSizeMB;
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

  session->Setup(ConfigureSession(bufferSizeMB));
  session->StartBlocking();
  impl_->session = std::move(session);
}

void G4TracingSession::Drain()
{
  G4AutoLock lock(&g4TracingSessionMutex);

  if (!impl_ || !impl_->session)
  {
    return;
  }

  // Stop-and-drain the current session, appending collected bytes to the open
  // file descriptor.  Then immediately restart a fresh session so tracing
  // continues without interruption.  The fd stays open between drain cycles;
  // only Stop() closes it.  Concatenated perfetto protobuf files are natively
  // valid (length-delimited TracePackets); perfetto trace_processor handles
  // multi-session files transparently.
  impl_->session->FlushBlocking(5000);
  impl_->session->StopBlocking();

  auto traceBytes = impl_->session->ReadTraceBlocking();
  impl_->session.reset();

  if (impl_->fd != G4TracingSessionImpl::kInvalidFd && !traceBytes.empty())
  {
    const char* ptr = traceBytes.data();
    std::size_t remaining = traceBytes.size();
    while (remaining > 0)
    {
#if defined(_WIN32)
      int written = _write(impl_->fd, ptr, static_cast<unsigned int>(remaining));
#else
      ssize_t written = write(impl_->fd, ptr, remaining);
#endif
      if (written <= 0) break;
      ptr += written;
      remaining -= static_cast<std::size_t>(written);
    }
  }

  // Restart tracing immediately.
  auto session = perfetto::Tracing::NewTrace();
  if (!session)
  {
    // If restart fails, close the file and give up.
    CloseTraceFile(impl_->fd);
    impl_.reset();
    return;
  }
  session->Setup(ConfigureSession(impl_->bufferSizeMB));
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

  // Perfetto SDK known issue (b/162206162): the last trace packet of each
  // producer thread needs an explicit Flush() to be committed to the SMB,
  // and a service-level FlushBlocking() to be scraped into the ring buffer
  // before stopping.  Worker threads call G4TracingSession::Flush() from
  // BeamOn() to commit their partial chunks; we then wait for the service
  // to scrape the SMB before calling StopBlocking().
  impl_->session->FlushBlocking(5000);
  impl_->session->StopBlocking();

  // Read all trace data from the in-memory ring buffer and write to file.
  // ReadTraceBlocking() returns all bytes collected after StopBlocking().
  auto traceBytes = impl_->session->ReadTraceBlocking();
  if (impl_->fd != G4TracingSessionImpl::kInvalidFd && !traceBytes.empty())
  {
    const char* ptr = traceBytes.data();
    std::size_t remaining = traceBytes.size();
    while (remaining > 0)
    {
#if defined(_WIN32)
      int written = _write(impl_->fd, ptr, static_cast<unsigned int>(remaining));
#else
      ssize_t written = write(impl_->fd, ptr, remaining);
#endif
      if (written <= 0) break;
      ptr += written;
      remaining -= static_cast<std::size_t>(written);
    }
    CloseTraceFile(impl_->fd);
    impl_->fd = G4TracingSessionImpl::kInvalidFd;
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
