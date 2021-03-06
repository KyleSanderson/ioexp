// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
#ifndef _include_amio_windows_errors_h_
#define _include_amio_windows_errors_h_

#include <amio.h>
#include "../shared/shared-errors.h"

namespace amio {

using namespace ke;

class WinError : public IOError
{
 public:
  WinError(); // Use GetLastError().
  explicit WinError(DWORD error);
  ~WinError();

  const char *Message() override;
  int ErrorCode() override {
    return error_;
  }
  ErrorType Type() {
    return ErrorType::System;
  }

 private:
  DWORD error_;
  char *message_;
};

class WinsockError : public WinError
{
 public:
  WinsockError(DWORD error) : WinError(error)
  {}
  WinsockError() : WinError(WSAGetLastError())
  {}
};

extern Ref<GenericError> eContextAlreadyAssociated;
extern Ref<GenericError> eInvalidContext;
extern Ref<GenericError> eLengthOutOfRange;
extern Ref<GenericError> eIncompatibleTransport;
extern Ref<GenericError> eSocketClosed;
extern Ref<GenericError> eIncompatibleSocket;
extern Ref<GenericError> eSocketAlreadyAttached;
extern Ref<GenericError> eImmediateDeliveryNotSupported;

} // namespace amio

#endif // _include_amio_windows_errors_h_
