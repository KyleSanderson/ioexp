// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
//
#include "include/amio.h"
#include "posix/amio-posix-transport.h"
#include "posix/amio-posix-errors.h"
#include "posix/amio-posix-select.h"
#if defined(AMIO_POLL_AVAILABLE)
# include "posix/amio-posix-poll.h"
#endif
#include <unistd.h>
#include <signal.h>

using namespace ke;
using namespace amio;

PassRef<IOError>
TransportFactory::CreateFromDescriptor(Ref<Transport> *outp, int fd, TransportFlags flags)
{
  Ref<IOError> error;
  Ref<PosixTransport> transport = new PosixTransport(fd, flags);
  if ((error = transport->Setup()) != nullptr)
    return error;
  *outp = transport;
  return nullptr;
}

PassRef<IOError>
TransportFactory::CreatePipe(Ref<Transport> *readerp, Ref<Transport> *writerp, TransportFlags flags)
{
  int fds[2];
  if (pipe(fds) == -1)
    return new PosixError();

  *readerp = new PosixTransport(fds[0], kTransportDefaultFlags);
  *writerp = new PosixTransport(fds[1], kTransportDefaultFlags);
  return nullptr;
}

PassRef<IOError>
PollerFactory::CreateSelectImpl(Poller **outp)
{
  *outp = new SelectImpl();
  return nullptr;
}

#if defined(AMIO_POLL_AVAILABLE)
PassRef<IOError>
PollerFactory::CreatePollImpl(Poller **outp)
{
  AutoPtr<PollImpl> poller(new PollImpl());
  Ref<IOError> error = poller->Initialize();
  if (error)
    return error;
  *outp = poller.take();
  return nullptr;
}
#endif

AutoDisableSigpipe::AutoDisableSigpipe()
{
  prev_handler_ = signal(SIGPIPE, SIG_IGN);
}

AutoDisableSigpipe::~AutoDisableSigpipe()
{
  signal(SIGPIPE, prev_handler_);
}

PassRef<IOError>
Poller::Attach(IPollable *pollable)
{
  return pollable->Attach(this);
}
