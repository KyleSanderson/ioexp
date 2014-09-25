// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
//
#include "posix/amio-posix-transport.h"
#include "posix/amio-posix-errors.h"
#include "posix/amio-posix-base-pump.h"
#include <unistd.h>
#include <errno.h>

using namespace amio;

PosixTransport::PosixTransport(int fd, TransportFlags flags)
 : fd_(fd),
   flags_(flags),
   pump_(nullptr)
{
}

PosixTransport::~PosixTransport()
{
  if (flags_ & kTransportAutoClose)
    Close();
}

void
PosixTransport::Close()
{
  if (fd_ == -1) {
    assert(!pump_);
    return;
  }

  if (pump_)
    pump_->unhook(this);

  close(fd_);
  fd_ = -1;
  pump_ = nullptr;
}

bool
PosixTransport::Read(IOResult *result, uint8_t *buffer, size_t maxlength)
{
  *result = IOResult();

  ssize_t rv = read(fd_, buffer, maxlength);
  if (rv == -1) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      if (pump_)
        pump_->onReadWouldBlock(this);
      return true;
    }

    result->Error = new PosixError();
    return false;
  }

  if (rv == 0) {
    pump_->unhook(this);
    result->Ended = true;
    return true;
  }

  result->Bytes = size_t(rv);
  return true;
}

bool
PosixTransport::Write(IOResult *result, const uint8_t *buffer, size_t maxlength)
{
  *result = IOResult();

  ssize_t rv = write(fd_, buffer, maxlength);
  if (rv == -1) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      if (pump_)
        pump_->onWriteWouldBlock(this);
      return true;
    }

    result->Error = new PosixError();
    return false;
  }

  result->Bytes = size_t(rv);
  return true;
}
