//***************************************************************************
// Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática *
// Departamento de Engenharia Electrotécnica e de Computadores              *
// Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                          *
//***************************************************************************
// Author: Ricardo Martins                                                  *
//***************************************************************************

// ISO C++ 98 headers.
#include <cerrno>

// DUNE headers.
#include <DUNE/Config.hpp>
#include <DUNE/Network/Address.hpp>
#include <DUNE/Network/UDPSocket.hpp>
#include <DUNE/Network/Exceptions.hpp>
#include <DUNE/Utils/ByteCopy.hpp>
#include <DUNE/System/IOMultiplexing.hpp>

// Win32 headers.
#if defined(DUNE_SYS_HAS_WINSOCK2_H)
#  include <winsock2.h>
#  define DUNE_SOCKET_ERROR System::Error::getLastMessage()
#endif

#if defined(DUNE_SYS_HAS_WS2TCPIP_H)
#  include <ws2tcpip.h>
#endif

// POSIX headers.
#if defined(DUNE_SYS_HAS_UNISTD_H)
#  include <unistd.h>
#endif

#if defined(DUNE_SYS_HAS_NETINET_IN_H)
#  include <netinet/in.h>
#endif

#if defined(DUNE_SYS_HAS_SYS_SOCKET_H)
#  include <sys/socket.h>
#  define DUNE_SOCKET_ERROR System::Error::getLastMessage()
#  define INVALID_SOCKET -1
#endif

#if defined(DUNE_SYS_HAS_ARPA_INET_H)
#  include <arpa/inet.h>
#endif

#if defined(DUNE_SYS_HAS_NETDB_H)
#  include <netdb.h>
#endif

#if defined(DUNE_OS_WINDOWS) && defined(DUNE_SYS_HAS_WSA_IOCTL)
// To avoid cumbersome Windows connection reset error 10054 (WSACONNRESET)
// Reference: http://support.microsoft.com/kb/263823
#  ifndef IOC_VENDOR
#    define IOC_VENDOR 0x18000000
#  endif
#  ifndef SIO_UDP_CONNRESET
#    define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#  endif
#endif

#if !defined(EHOSTUNREACH) && defined(WSAEHOSTUNREACH)
#  define EHOSTUNREACH WSAEHOSTUNREACH
#endif

#if !defined(ENETUNREACH) && defined(WSAENETUNREACH)
#  define ENETUNREACH WSAENETUNREACH
#endif

namespace DUNE
{
  namespace Network
  {
    UDPSocket::UDPSocket(void)
    {
      //  POSIX / Win32
#if defined(DUNE_SYS_HAS_SOCKET)
      m_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if (m_handle == INVALID_SOCKET)
        throw NetworkError(DTR("unable to create socket"), DUNE_SOCKET_ERROR);

#  if defined(DUNE_OS_WINDOWS) && defined(DUNE_SYS_HAS_WSA_IOCTL)
      // To avoid cumbersome Windows connection reset error 10054 (WSACONNRESET)
      // Reference: http://support.microsoft.com/kb/263823
      DWORD dummy = 0;
      BOOL behavior = FALSE;

      if (WSAIoctl(m_handle, SIO_UDP_CONNRESET, &behavior, sizeof(behavior),
                   0, 0, &dummy, 0, 0) == SOCKET_ERROR)
      {
        if (GetLastError() != WSAEOPNOTSUPP)
        {
          throw NetworkError(DTR("setting up socket"), DUNE_SOCKET_ERROR);
        }
      }
#  endif
#else
      throw NotImplemented("UDPSocket");
#endif
    }

    UDPSocket::~UDPSocket(void)
    {
      // POSIX
#if defined(DUNE_SYS_HAS_CLOSE)
      close(m_handle);

      // Win32
#elif defined(DUNE_SYS_HAS_CLOSESOCKET)
      closesocket(m_handle);
#endif
    }

    void
    UDPSocket::enableBroadcast(bool value)
    {
      int on = value ? 1 : 0;
      setsockopt(m_handle, SOL_SOCKET, SO_BROADCAST, (char*)&on, sizeof(int));
    }

    void
    UDPSocket::setMulticastTTL(uint8_t value)
    {
      setsockopt(m_handle, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&value, sizeof(value));
    }

    void
    UDPSocket::setMulticastLoop(bool value)
    {
      char loop = value ? 1 : 0;
      setsockopt(m_handle, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
    }

    void
    UDPSocket::joinMulticastGroup(Address group, Address itf)
    {
      ip_mreq mreq;

      mreq.imr_multiaddr.s_addr = group.toInteger();
      mreq.imr_interface.s_addr = itf.toInteger();

      setsockopt(m_handle, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
    }

    void
    UDPSocket::bind(uint16_t port, Address add, bool reuse)
    {
      if (reuse)
      {
        int on = 1;
        setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(int));
      }

      sockaddr_in self;

      self.sin_family = AF_INET;
      self.sin_port = Utils::ByteCopy::toBE(port);
      self.sin_addr.s_addr = add.toInteger();

      if (::bind(m_handle, (::sockaddr*)&self, sizeof(self)) < 0)
        throw NetworkError(DTR("unable to bind to socket"), DUNE_SOCKET_ERROR);
    }

    int
    UDPSocket::write(const char* buffer, int len, const Address& host, uint16_t port)
    {
      sockaddr_in host_sai;
      socklen_t sock_len = sizeof(host_sai);

      host_sai.sin_family = AF_INET;
      host_sai.sin_port = Utils::ByteCopy::toBE(port);
      host_sai.sin_addr.s_addr = host.toInteger();

      int rv = sendto(m_handle, buffer, len, 0, (::sockaddr*)&host_sai, (::socklen_t)sock_len);

      if (rv == -1)
      {
        if (errno == EHOSTUNREACH)
          throw HostUnreachable(host.str());
        else if (errno == ENETUNREACH)
          throw NetworkUnreachable(host.str());
        else
          throw NetworkError(DTR("error sending data"), DUNE_SOCKET_ERROR);
      }

      return rv;
    }

    int
    UDPSocket::read(char* buffer, int len, Address* add)
    {
      sockaddr_in host;
      socklen_t sock_len = sizeof(host);
      std::memset((char*)&host, 0, sock_len);

      int rv = recvfrom(m_handle, buffer, len, 0, (::sockaddr*)&host, (::socklen_t*)&sock_len);

      if (rv <= 0)
        throw NetworkError(DTR("error receiving data"), DUNE_SOCKET_ERROR);

      if (add != NULL)
        *add = (::sockaddr*)&host;

      return rv;
    }

    void
    UDPSocket::addToPoll(System::IOMultiplexing& poller)
    {
      poller.add(&m_handle);
    }

    void
    UDPSocket::delFromPoll(System::IOMultiplexing& poller)
    {
      poller.del(&m_handle);
    }

    bool
    UDPSocket::wasTriggered(System::IOMultiplexing& poller)
    {
      return poller.wasTriggered(&m_handle);
    }
  }
}