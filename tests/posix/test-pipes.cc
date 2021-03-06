// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson, AlliedModders LLC
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
//
#include <amio.h>
#include <string.h>
#include "test-pipes.h"

using namespace ke;
using namespace amio;

TestPipes::TestPipes(CreatePoller_t ctor, const char *name)
 : Test(name),
   constructor_(ctor)
{
}

bool
TestPipes::setup(EventMode mode)
{
  reset();

  Ref<IOError> error = TransportFactory::CreatePipe(&reader_, &writer_);
  if (!check_error(error, "create pipes"))
    return false;

  error = poller_->Attach(reader_, this, Events::Read, mode);
  if (!check_error(error, "attach read pipe"))
    return false;
  error = poller_->Attach(writer_, this, Events::Write, mode);
  if (!check_error(error, "attach write pipe"))
    return false;

  got_read_ = false;
  got_write_ = false;
  got_hangup_ = false;
  return true;
}

void
TestPipes::reset()
{
  if (reader_)
    poller_->Detach(reader_);
  if (writer_)
    poller_->Detach(writer_);
  reader_ = nullptr;
  writer_ = nullptr;
}

bool
TestPipes::Run()
{
  Ref<IOError> error = constructor_(&poller_);
  if (!check_error(error, "create poller"))
    return false;

  if (!test_read_write())
    return false;
  if (!test_poll_write_close())
    return false;
  if (!test_poll_read_close())
    return false;
  if (!test_sticky())
    return false;
  if (!test_edge_triggering())
    return false;

  reset();
  poller_ = nullptr;
  return true;
}

bool
TestPipes::wait_for_read()
{
  while (!got_read_) {
    if (!check_error(poller_->Poll(), "poll for read"))
      return false;
    if (!check(!got_hangup_, "should not get hangup"))
      return false;
  }
  return true;
}

bool
TestPipes::wait_for_write()
{
  while (!got_write_) {
    if (!check_error(poller_->Poll(), "poll for write"))
      return false;
    if (!check(!got_hangup_, "should not get hangup"))
      return false;
  }
  return true;
}

bool
TestPipes::test_sticky()
{
  AutoTestContext test("sticky events");
  if (!setup(EventMode::Level))
    return false;

  // We should get a write event.
  if (!check_error(poller_->Poll(), "initial poll"))
    return false;
  if (!check(got_write_, "should receive initial write"))
    return false;

  got_write_ = false;
  if (!check_error(poller_->Poll(kSafeTimeout), "second poll"))
    return false;
  if (!check(got_write_, "should receive second write"))
    return false;
  if (!write("a", 1))
    return false;

  got_read_ = false;
  if (!check_error(poller_->Poll(kSafeTimeout), "third poll"))
    return false;
  if (!check(got_read_, "should have gotten read"))
    return false;
  got_read_ = false;
  if (!check_error(poller_->Poll(kSafeTimeout), "fourth poll"))
    return false;
  if (!check(got_read_, "should have gotten read"))
    return false;

  if (!check_error(poller_->ChangeEvents(reader_, Events::None), "change events"))
    return false;

  got_read_ = false;
  if (!check_error(poller_->Poll(kSafeTimeout), "fourth poll"))
    return false;
  if (!check(!got_read_, "should not have gotten read"))
    return false;

  return true;
}

bool
TestPipes::test_edge_triggering()
{
  AutoTestContext test("edge-triggering");
  if (!setup(EventMode::ETS))
    return false;

  if (!check_error(poller_->Poll(), "initial poll"))
    return false;
  if (!check(got_write_, "should receive initial write"))
    return false;

  got_write_ = false;
  got_read_ = false;
  if (!check_error(poller_->Poll(kSafeTimeout), "initial poll"))
    return false;
  if (!check(!got_write_, "should not have gotten a write"))
    return false;
  if (!check(!got_read_, "should not have gotten a read"))
    return false;

  return true;
}

bool
TestPipes::test_read_write()
{
  AutoTestContext test("reading and writing");
  if (!setup(EventMode::ETS))
    return false;

  if (!check_error(poller_->Poll(), "initial poll"))
    return false;
  if (!check(!got_hangup_, "should not receive hangup"))
    return false;
  if (!check(!got_read_, "should not receive read"))
    return false;
  if (!check(got_write_, "should receive write"))
    return false;

  // Write.
  if (!write("hello", 5))
    return false;

  // Read.
  size_t nread = 0;
  char buffer[5];
  while (true) {
    if (!wait_for_read())
      return false;

    IOResult r;
    if (!check(reader_->Read(&r, buffer, sizeof(buffer)), "read from pipe"))
      return false;
    if (!r.completed) {
      got_read_ = false;
      continue;
    }

    nread += r.bytes;
    if (nread == sizeof(buffer))
      break;
  }
  if (!check(memcmp(buffer, "hello", 5) == 0, "got bytes"))
    return false;

  // Close the other side.
  writer_->Close();
  writer_ = nullptr;

  // We should be ready to read again.
  assert(got_read_);
  IOResult r;
  if (!check(reader_->Read(&r, buffer, sizeof(buffer)), "read from closed pipe"))
    return false;
  if (!check(r.ended, "should have gotten EOF from read pipe"))
    return false;

  return true;
}

bool
TestPipes::test_poll_write_close()
{
  AutoTestContext test("polling after a writer is closed");
  if (!setup(EventMode::ETS))
    return false;

  writer_->Close();
  if (!check_error(poller_->Poll(), "poll after write close"))
    return false;
  if (got_hangup_) {
    if (!check_error(got_error_, "got clean hangup"))
      return false;
    return true;
  }
  if (!check(got_read_, "should have gotten read-ready"))
    return false;

  IOResult r;
  char buffer[1];
  if (!check(reader_->Read(&r, buffer, sizeof(buffer)), "read from closed pipe"))
    return false;
  if (!check(r.ended, "should have gotten EOF from read pipe"))
    return false;

  return true;
}

bool
TestPipes::test_poll_read_close()
{
  AutoTestContext test("polling after a reader is closed");
  if (!setup(EventMode::ETS))
    return false;

  reader_->Close();
  if (!check_error(poller_->Poll(), "poll after read close"))
    return false;
  if (got_hangup_) {
    // kqueue() does not give us an error here.
#if 0
    if (!check(got_error_ != nullptr, "got unclean hangup"))
      return false;
#endif
    return true;
  }

  // We should get an error trying to write.
  IOResult r;
  char buffer[1] = {0};
  if (!check(!writer_->Write(&r, buffer, sizeof(buffer)), "write to closed pipe"))
    return false;
  if (!check(r.error != nullptr, "got error"))
    return false;

  return true;
}

bool
TestPipes::write(const char *msg, size_t len)
{
  // Write.
  size_t nwritten = 0;
  while (true) {
    IOResult r;
    if (!check(writer_->Write(&r, msg + nwritten, len - nwritten), "write to pipe"))
      return false;
    nwritten += r.bytes;
    if (nwritten == len)
      break;

    got_write_ = false;
    if (!wait_for_write())
      return false;
  }
  return true;
}
