// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
//
#ifndef _include_amio_net_h_
#define _include_amio_net_h_

#include <amio.h>
#include <am-platform.h>
#include <am-string.h>
#include <limits.h>
#if defined(KE_POSIX)
# include <string.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <sys/un.h>
#elif defined(KE_WINDOWS)
# include <Winsock2.h>
# include <ws2tcpip.h>
#endif

#if defined(KE_WINDOWS)
typedef int socklen_t;
#endif

namespace amio {
namespace net {

using namespace amio;

// Networks types.
enum class AddressFamily
{
  IPv4,     // IPv4 address.
  IPv6,     // IPv6 address.
  Unix,     // Unix domain sockets (not available on Windows).
  Unknown   // Unknown address family.
};

// Supported protocols.
enum class Protocol
{
  TCP,      // Streaming for IPv4 or IPv6.
  UDP,      // Datagrams for IPv4 or IPv6.
  Stream,   // Any stream protocol available. With AF::IP, this is TCP.
            // With AF::Unix, it is a byte stream protocol.
  Datagram, // Any datagram protocol available. With AF::IP, this is TCP.
            // With AF::Unix, it is a message protocol.
  Unknown
};

// Forward declarations for Address types.
class IPAddress;
class IPv4Address;
class IPv6Address;
class UnixAddress;

// Abstract representation of a network address.
class AMIO_LINK Address : public ke::Refcounted<Address>
{
 public:
  virtual ~Address()
  {}

  // Get the "any" address for an address family. This is 0.0.0.0 for IPv4,
  // and :: for IPv6. There is no such address for Unix addresses.
  static PassRef<IOError> AnyAddress(Ref<Address> *outp, AddressFamily af);

  // Resolve an address. AMIO cannot guarantee non-blocking resolution ability,
  // so take care: Resolve() can block.
  //
  // IP address strings may end with a ":port" string, where port can be a
  // service name or a numerical port. If |af| is Unknown, then resolution can
  // pick any available address family.
  //
  // IPv6 addresses, if specifying a port or service, should be encased in
  // braces. I.e. [0:0:0:0::0]:80.
  //
  // Returns nullptr with no error if the address could not be resolved.
  static PassRef<IOError> Resolve(Ref<Address> *outp, AddressFamily af, const char *address);

  // Return the address family.
  virtual AddressFamily Family() = 0;

  // Format the address as a string. Returns the number of bytes written,
  // not including the null terminator.
  virtual ke::AString ToString() = 0;

  // Return a sockaddr representing the address.
  virtual const struct sockaddr *SockAddr() const = 0;

  // Returns the length of the sockaddr.
  virtual socklen_t SockAddrLen() = 0;

  // Create a new Address of the same address family, and provide a mutable
  // buffer to initialize its sockaddr. This is intended for callers of
  // accept(). Since the buffer is left uninitialized, it is the user's
  // responsibility to make sure it is filled. Note that callers should use
  // lenp for the buffer length, not SockAddrLen(), which requires an
  // initialized buffer.
  virtual PassRef<Address> NewBuffer(struct sockaddr **outp, socklen_t *lenp) = 0;

  // Copy the address.
  virtual PassRef<Address> Copy();

  // These functions can be used for casting to derived types, without using
  // dynamic_cast. The |toX| variants will assert if they're the wrong type.
  // The |asX| variants will return nullptr.
  virtual PassRef<IPAddress> asIPAddress();
  virtual PassRef<IPv4Address> asIPv4Address();
  virtual PassRef<IPv6Address> asIPv6Address();

  PassRef<IPAddress> toIPAddress();
  PassRef<IPv4Address> toIPv4Address();
  PassRef<IPv6Address> toIPv6Address();

#if defined(KE_POSIX)
  virtual PassRef<UnixAddress> asUnixAddress();
  PassRef<UnixAddress> toUnixAddress();
#endif
};

// Either an IPv4 or an IPv6 address.
class AMIO_LINK IPAddress : public Address
{
 public:
  // Return the port this address is on; 0 if none.
  virtual int Port() const = 0;

  virtual PassRef<IPAddress> asIPAddress() override {
    return this;
  }
};

// An IPv4 address.
class AMIO_LINK IPv4Address : public IPAddress
{
 public:
  IPv4Address(struct sockaddr **buf, socklen_t *buflen);
  IPv4Address(const struct sockaddr_in &buf);

  // Resolve an IPv4 address. AMIO cannot guarantee non-blocking resolution,
  // so take care: Resolve() can block.
  //
  // Returns nullptr with no error if the address could not be resolved.
  static PassRef<IOError> Resolve(Ref<IPv4Address> *outp, const char *address);

  // Returns an address for 0.0.0.0.
  static PassRef<IOError> AnyAddress(Ref<IPv4Address> *outp);

  AddressFamily Family() override {
    return AddressFamily::IPv4;
  }
  const struct sockaddr *SockAddr() const override {
    return reinterpret_cast<const sockaddr *>(&buf_);
  }
  socklen_t SockAddrLen() override {
    return sizeof(buf_);
  }
  int Port() const override {
    return buf_.sin_port;
  }
  virtual PassRef<IPv4Address> asIPv4Address() override {
    return this;
  }

  PassRef<Address> NewBuffer(struct sockaddr **outp, socklen_t *lenp) override;
  ke::AString ToString() override;

 private:
  IPv4Address();

 private:
  struct sockaddr_in buf_;
};

// An IPv6 address.
class AMIO_LINK IPv6Address : public IPAddress
{
 public:
  IPv6Address();
  IPv6Address(struct sockaddr **buf, socklen_t *buflen);
  IPv6Address(const struct sockaddr_in6 &buf);

  // Resolve an IPv6 address. AMIO cannot guarantee non-blocking resolution
  // ability, so take care: Resolve() can block.
  static PassRef<IOError> Resolve(Ref<IPv6Address> *outp, const char *address);

  // Returns an address for IPv6.
  static PassRef<IOError> AnyAddress(Ref<IPv6Address> *outp);

  AddressFamily Family() override {
    return AddressFamily::IPv6;
  }
  const struct sockaddr *SockAddr() const override {
    return reinterpret_cast<const sockaddr *>(&buf_);
  }
  socklen_t SockAddrLen() override {
    return sizeof(buf_);
  }
  int Port() const override {
    return buf_.sin6_port;
  }
  virtual PassRef<IPv6Address> asIPv6Address() override {
    return this;
  }

  PassRef<Address> NewBuffer(struct sockaddr **outp, socklen_t *lenp) override;
  ke::AString ToString() override;

 private:
  struct sockaddr_in6 buf_;
};

#if defined(KE_POSIX)
// A Unix address.
class UnixAddress : public Address
{
 public:
  UnixAddress();
  UnixAddress(struct sockaddr **buf, socklen_t *buflen);

  static PassRef<IOError> Resolve(Ref<UnixAddress> *outp, const char *address);

  AddressFamily Family() override {
    return AddressFamily::Unix;
  }
  const struct sockaddr *SockAddr() const override {
    return reinterpret_cast<const sockaddr *>(&buf_);
  }
  virtual PassRef<UnixAddress> asUnixAddress() override {
    return this;
  }

  PassRef<Address> NewBuffer(struct sockaddr **outp, socklen_t *lenp) override;
  socklen_t SockAddrLen() override;
  ke::AString ToString() override;

 private:
  struct sockaddr_un buf_;
};
#endif

class AMIO_LINK Connection : public ke::IRefcounted
{
 public:
  // Return the local address of the connection.
  virtual PassRef<IOError> LocalAddress(Ref<Address> *outp) = 0;

  // Return the peer address of the connection.
  virtual PassRef<IOError> PeerAddress(Ref<Address> *outp) = 0;

  // Return the underlying transport.
  virtual PassRef<Transport> GetTransport() = 0;
};

enum class Action
{
  // Instructs the I/O layer to try and consume another connection, as long
  // as it would not block to do so. Note that, like all edge-triggered APIs,
  // this risks starving other I/O operations on the same thread.
  Again,

  // Instructs the I/O layer to defer any pending connection notifications
  // until the next call to Poll().
  DeferNext
};

enum class Severity {
  // Some kind of network error occurred accepting a connection, but the
  // server socket is otherwise fine.
  Warning,

  // Either ENOMEM or EMFILE. The server is likely to keep receiving this
  // error frequently, and the application should consider shutting down.
  Severe,

  // The server socket can no longer be used, and no more connections will
  // be received. This usually indicates the socket has been closed.
  Fatal
};

// A server accepts network connections on a connection-oriented port.
class AMIO_LINK Server : public ke::IRefcounted
{
 public:
  // Events on this listener can be fired upon polling.
  class AMIO_LINK Listener : public ke::IRefcounted
  {
   public:
    virtual ~Listener()
    {}

    // Called when a new connection is available.
    virtual Action Accept(Ref<Connection> conn) {
      return Action::DeferNext;
    }

    // Called when an error occurs accepting connections.
    virtual void OnError(Ref<IOError> error, Severity severity)
    {}
  };

  // Create a new server on the given address. On success, a non-null server
  // is returned. It can be added to a poller to begin receiving events on
  // the given listener.
  //
  // Backlog specifies the maximum number of pending connections that can be
  // enqueued. Use 0 for the default (usually 128).
  static PassRef<IOError> Create(
    Ref<Server> *server,
    Ref<IODispatcher> dispatcher,
    Ref<Address> address,
    Protocol protocol,
    Ref<Server::Listener> listener,
    unsigned backlog = 0
  );

  // Return the address the server is listening on.
  virtual PassRef<Address> ListenAddress() = 0;

  // Close the server; stops accepting requests, and terminates any outstanding
  // connections. This must not be called if any calls to Poll() are in
  // progress on another thread.
  virtual void Close() = 0;
};

class AMIO_LINK Operation : public ke::IRefcounted
{
 public:
  // Cancel the operation. Once Cancel() is called, it is guaranteed that an
  // in-progress connection operation will not complete. If the operation has
  // already completed, it will still fire an OnConnect event. Otherwise, no
  // callbacks will fire (including OnConnectFailed).
  virtual void Cancel() = 0;
};

class Client
{
 public:
  // Events on this listener can be fired upon polling.
  class Listener : public Poller::Listener
  {
   public:
    // Called when the socket is connected.
    virtual void OnConnect(Ref<Connection> transport) = 0;

    // Called if the socket fails to connect.
    virtual void OnConnectFailed(Ref<IOError> error) = 0;
  };

  struct Result {
    // If the connection completed immediately, this will be set.
    Ref<Connection> connection;

    // If the connection is being processed asynchronously, this object allows
    // it to be cancelled.
    Ref<Operation> operation;
  };

  // Initiates a connection to an address and returns the result of the
  // operation. If the operation fails to initiate, error is returned.
  //
  // If a connection could be immediately established, the connection object
  // is returned in |result|. Otherwise the |operation| field is set, and
  // unless cancelled, OnConnect or OnConnectFailed will fire with a later
  // Poll().
  //
  // Parameters:
  //  result:    Result output.
  //  poller:    The poller to attach to.
  //  address:   The address to connect to upon attaching.
  //  protocol:  The protocol to use for the socket.
  //  listener:  Listener to receive event callbacks.
  //  events:    The initial events to listen for once the client has connected.
  //             Ignored for Windows IOCP.
  //  mode:      The event mode to use. Ignored for Windows IOCP.
  static PassRef<IOError> Create(
    Result *result,
    Ref<IODispatcher> dispatcher,
    Ref<Address> address,
    Protocol protocol,
    Ref<Client::Listener> listener,
    Events events = Events::None,
    EventMode mode = EventMode::Default
  );
};

// Recommended UDP packet size.
static const size_t kDefaultDatagramSize = 1472;

// Maximum theoretical sizes of UDP packets.
static const size_t kMaxUDPv4PacketSize = 65507;
static const size_t kMaxUDPv6PacketSize = 65535;

// Access for creating raw sockets. The address version creates a bound socket.
// On Windows, the sockets are overlapped so they can be attached to pollers.
AMIO_LINK Ref<IOError> CreateSocket(Ref<Transport> *outp, AddressFamily af, Protocol proto);
AMIO_LINK Ref<IOError> CreateSocket(Ref<Transport> *outp, Ref<Address> address, Protocol proto);

// Creates a connection to an address. This call will block while making the
// connection (if the protocol is connection-oriented). Afterward, the
// transport is in non-blocking mode so it can be used with Pollers.
AMIO_LINK Ref<IOError> ConnectTo(Ref<Connection> *outp, Protocol protocol, Ref<Address> address);

// Start up the networking library.
PassRef<IOError> StartNetworking();

} // net
} // amio

#endif // _include_amio_net_h_
