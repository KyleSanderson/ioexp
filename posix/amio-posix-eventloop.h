// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
//
#ifndef _include_amio_eventloop_h_
#define _include_amio_eventloop_h_

#include <amio-eventloop.h>
#include "../shared/amio-task-queue.h"

namespace amio {

using namespace ke;

class PosixEventLoopForIO
 : public EventLoopForIO,
   public ke::RefcountedThreadsafe<PosixEventLoopForIO>
{
 public:
  PosixEventLoopForIO(Ref<Poller> poller);
  ~PosixEventLoopForIO();

  KE_IMPL_REFCOUNTING_TS(PosixEventLoopForIO);

  Ref<IOError> Initialize();

 public:
  void PostTask(Task *task) override;
  void PostQuit() override;
  bool ShouldQuit() override;
  void Loop() override;
  void NotifyTask() override;
  void NotifyQuit() override;

  PassRef<IOError> Attach(Ref<Transport> transport, Ref<StatusListener> listener,
                          Events events, EventMode mode) override;
  void Detach(Ref<Transport> transport) override;
  PassRef<IOError> ChangeEvents(Ref<Transport> transport, Events events) override;
  PassRef<IOError> AddEvents(Ref<Transport> transport, Events events) override;
  PassRef<IOError> RemoveEvents(Ref<Transport> transport, Events events) override;
  void Shutdown() override;

  PassRef<Poller> GetPoller() override {
    return poller_;
  }

 private:
  class Wakeup
   : public StatusListener,
     public ke::Refcounted<Wakeup>
  {
   public:
    Wakeup(PosixEventLoopForIO *parent)
     : parent_(parent),
       write_blocked_(false)
    {}

    KE_IMPL_REFCOUNTING(Wakeup);

    void OnReadReady() override;
    void OnWriteReady() override;

    void disable() {
      parent_ = nullptr;
    }

   private:
    PosixEventLoopForIO *parent_;
    bool write_blocked_;
  };

 private:
  Ref<Poller> poller_;	
  AutoPtr<TaskQueueImpl> tasks_;
  Ref<Transport> read_pipe_;
  Ref<Transport> write_pipe_;
  Ref<Wakeup> wakeup_;
  AutoPtr<EventQueue> event_queue_;
};

} // namespace amio

#endif // _include_amio_eventloop_h_
