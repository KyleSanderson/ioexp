// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
//
#include "posix/amio-posix-errors.h"
#include "linux/amio-linux.h"
#include "linux/amio-linux-epoll.h"
#include <unistd.h>
#include <sys/epoll.h>

#if !defined(EPOLLRDHUP)
# define EPOLLRDHUP 0x2000
#endif

using namespace ke;
using namespace amio;

// This is passed to the kernel, which ignores it. But it has to be non-zero.
static const size_t kInitialEpollSize = 16;

// The maximum number of events to receive on a poll operation.
static const size_t kMaxPollEvents = 256;

EpollMessagePump::EpollMessagePump(size_t maxEvents)
 : ep_(-1),
   can_use_rdhup_(false),
   generation_(0),
   max_events_(maxEvents)
{
#if defined(__linux__)
  if (IsAtLeastLinux(2, 6, 17))
    can_use_rdhup_ = true;
#endif
}

Ref<IOError>
EpollMessagePump::Initialize()
{
  if ((ep_ = epoll_create(kInitialEpollSize)) == -1)
    return new PosixError();

  event_buffer_ = new epoll_event[max_events_];
  if (!event_buffer_)
    return eOutOfMemory;

  return nullptr;
}

EpollMessagePump::~EpollMessagePump()
{
  if (ep_ == -1)
    return;
  close(ep_);
}

Ref<IOError>
EpollMessagePump::Register(Ref<Transport> baseTransport, Ref<StatusListener> listener)
{
  Ref<PosixTransport> transport(baseTransport->toPosixTransport());
  if (!transport)
    return eIncompatibleTransport;
  if (transport->pump())
    return eTransportAlreadyRegistered;
  if (transport->fd() == -1)
    return eTransportClosed;

  assert(listener);

  size_t slot;
  if (!free_slots_.empty()) {
    slot = free_slots_.popCopy();
  } else {
    slot = listeners_.length();
    if (!listeners_.append(PollData()))
      return eOutOfMemory;
  }

  // By default we wait for reads (see the comment in the select pump).
  int defaultEvents = EPOLLIN | EPOLLOUT | EPOLLET;
  if (can_use_rdhup_)
    defaultEvents |= EPOLLRDHUP;

  // Hook up events.
  epoll_event pe;
  pe.events = defaultEvents;
  pe.data.ptr = (void *)slot;
  if (epoll_ctl(ep_, EPOLL_CTL_ADD, transport->fd(), &pe) == -1) {
    Ref<IOError> err = new PosixError();
    free_slots_.append(slot);
    return err;
  }

  // Hook up the transport.
  listeners_[slot].transport = transport;
  listeners_[slot].listener = listener;
  listeners_[slot].modified = generation_;
  transport->setPump(this);
  transport->setUserData(slot);
  return nullptr;
}

void
EpollMessagePump::Deregister(Ref<Transport> baseTransport)
{
  Ref<PosixTransport> transport(baseTransport->toPosixTransport());
  if (!transport || transport->pump() != this || transport->fd() == -1)
    return;

  unhook(transport);
}

Ref<IOError>
EpollMessagePump::Poll(int timeoutMs)
{
  int nevents = epoll_wait(ep_, event_buffer_, max_events_, timeoutMs);
  if (nevents == -1)
    return new PosixError();

  generation_++;
  for (int i = 0; i < nevents; i++) {
    epoll_event &ep = event_buffer_[i];
    size_t slot = (size_t)ep.data.ptr;
    if (!isEventValid(slot))
      continue;

    // Handle errors first.
    if (ep.events & EPOLLERR) {
      PollData data = listeners_[slot];
      unhook(data.transport);
      data.listener->OnError(data.transport, eUnknownHangup);
      continue;
    }

    // Prioritize EPOLLIN over EPOLLHUP/EPOLLRDHUP.
    if (ep.events & EPOLLIN) {
      listeners_[slot].listener->OnReadReady(listeners_[slot].transport);
      if (!isEventValid(slot))
        continue;
    }

    // Handle explicit hangup.
    if (ep.events & (EPOLLRDHUP|EPOLLHUP)) {
      PollData data = listeners_[slot];
      unhook(data.transport);
      data.listener->OnHangup(data.transport);
      continue;
    }

    // Handle output.
    if (ep.events & EPOLLOUT) {
      // No need to check if the event is still valid since this is the last
      // check.
      listeners_[slot].listener->OnWriteReady(listeners_[slot].transport);
    }
  }

  return nullptr;
}

void
EpollMessagePump::Interrupt()
{
  // Not yet implemented.
  abort();
}

void
EpollMessagePump::onReadWouldBlock(int fd)
{
  // Do nothing... epoll is edge-triggered.
}

void
EpollMessagePump::onWriteWouldBlock(int fd)
{
  // Do nothing... epoll is edge-triggered.
}

void
EpollMessagePump::unhook(Ref<PosixTransport> transport)
{
  assert(transport->pump() == this);

  int fd = transport->fd();
  uintptr_t slot = transport->getUserData();
  assert(fd != -1);
  assert(listeners_[slot].transport == transport);

  // For compatibility with older Linux kernels we use a bogus event pointer
  // instead of nullptr.
  epoll_event pe;
  epoll_ctl(ep_, EPOLL_CTL_DEL, fd, &pe);

  listeners_[slot].transport = nullptr;
  listeners_[slot].listener = nullptr;
  listeners_[slot].modified = generation_;
  transport->setPump(nullptr);
  free_slots_.append(slot);
}
