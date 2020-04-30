/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.

* Redistributions in binary form must reproduce the
above copyright notice, this list of conditions
and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
nor the names of its contributors may be used to
endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
****************************************************************************/

/****************************************************************************
written by
Yunhong Gu, last updated 01/27/2011
*****************************************************************************/

#ifndef _WIN32
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef LEGACY_WIN32
#include <wspiapi.h>
#endif
#endif

#include <cassert>

#include "channel.h"
#include "core.h"
#include "common.h"
#include "packet.h"

#ifdef _WIN32
#   define socklen_t int
#endif

#ifndef _WIN32
#   define NET_ERROR errno
#else
#   define NET_ERROR WSAGetLastError()
#endif

//-------------------------------------------------------------------------------------------------

std::unique_ptr<AbstractUdpChannel> UdpChannelFactory::create(int ipVersion)
{
    if (m_customFunc)
        return m_customFunc(ipVersion);
    else
        return std::make_unique<UdpChannel>(ipVersion);
}

UdpChannelFactory::FactoryFunc UdpChannelFactory::setCustomFunc(
    FactoryFunc func)
{
    return std::exchange(m_customFunc, std::move(func));
}

UdpChannelFactory& UdpChannelFactory::instance()
{
    static UdpChannelFactory instance;
    return instance;
}

//-------------------------------------------------------------------------------------------------

UdpChannel::UdpChannel(int version):
    m_iIPversion(version),
    m_iSocket(INVALID_UDP_SOCKET),
    m_iSndBufSize(65536),
    m_iRcvBufSize(65536)
{
}

UdpChannel::~UdpChannel()
{
    closeSocket();
}

Result<> UdpChannel::open(const std::optional<detail::SocketAddress>& addr)
{
    // construct an socket
    m_iSocket = ::socket(m_iIPversion, SOCK_DGRAM, 0);
    if (INVALID_UDP_SOCKET == m_iSocket)
        return OsError();

    if (addr)
    {
        if (0 != ::bind(m_iSocket, addr->get(), addr->size()))
            return OsError();
    }
    else
    {
        //sendto or WSASendTo will also automatically bind the socket
        addrinfo hints;
        addrinfo* res;

        memset(&hints, 0, sizeof(struct addrinfo));

        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = m_iIPversion;
        hints.ai_socktype = SOCK_DGRAM;

        if (0 != ::getaddrinfo(NULL, "0", &hints, &res))
            return OsError();

        if (0 != ::bind(m_iSocket, res->ai_addr, res->ai_addrlen))
            return OsError();

        ::freeaddrinfo(res);
    }

    return setUDPSockOpt();
}

Result<> UdpChannel::open(UDPSOCKET udpsock)
{
    m_iSocket = udpsock;

    return setUDPSockOpt();
}

Result<> UdpChannel::setUDPSockOpt()
{
#if defined(BSD) || defined(__APPLE__)
    // BSD system will fail setsockopt if the requested buffer size exceeds system maximum value
    int maxsize = 64000;
    if (0 != ::setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char*)&m_iRcvBufSize, sizeof(int)))
        ::setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char*)&maxsize, sizeof(int));
    if (0 != ::setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char*)&m_iSndBufSize, sizeof(int)))
        ::setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char*)&maxsize, sizeof(int));
#else
    // for other systems, if requested is greated than maximum, the maximum value will be automactally used
    if ((0 != ::setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char*)&m_iRcvBufSize, sizeof(int))) ||
        (0 != ::setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char*)&m_iSndBufSize, sizeof(int))))
    {
        return OsError();
    }
#endif

    timeval tv;
    tv.tv_sec = 0;
#if defined (BSD) || defined (__APPLE__)
    // Known BSD bug as the day I wrote this code.
    // A small time out value will cause the socket to block forever.
    tv.tv_usec = 10000;
#else
    tv.tv_usec = 100;
#endif

#ifdef UNIX
    // Set non-blocking I/O
    // UNIX does not support SO_RCVTIMEO
    int opts = ::fcntl(m_iSocket, F_GETFL);
    if (-1 == ::fcntl(m_iSocket, F_SETFL, opts | O_NONBLOCK))
        return OsError();
#elif _WIN32
    DWORD ot = 1; //milliseconds
    if (0 != ::setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&ot, sizeof(DWORD)))
        return OsError();
#else
    // Set receiving time-out value
    if (0 != ::setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(timeval)))
        return OsError();
#endif

    return success();
}

int UdpChannel::getSndBufSize()
{
    socklen_t size = sizeof(socklen_t);
    ::getsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, &size);
    return m_iSndBufSize;
}

int UdpChannel::getRcvBufSize()
{
    socklen_t size = sizeof(socklen_t);
    ::getsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, &size);
    return m_iRcvBufSize;
}

void UdpChannel::setSndBufSize(int size)
{
    m_iSndBufSize = size;
}

void UdpChannel::setRcvBufSize(int size)
{
    m_iRcvBufSize = size;
}

detail::SocketAddress UdpChannel::getSockAddr() const
{
    detail::SocketAddress socketAddress;
    ::getsockname(m_iSocket, socketAddress.get(), &socketAddress.length());
    return socketAddress;
}

Result<int> UdpChannel::sendto(const detail::SocketAddress& addr, CPacket packet)
{
    assert(m_iSocket != INVALID_UDP_SOCKET);

    // converting packet into network order
    encodePacket(&packet);

    auto [bufs, bufsCount] = packet.ioBufs();

#ifndef _WIN32
    detail::SocketAddress localAddr(addr);

    msghdr mh;
    mh.msg_name = localAddr.get();
    mh.msg_namelen = localAddr.size();
    mh.msg_iov = bufs;
    mh.msg_iovlen = bufsCount;
    mh.msg_control = NULL;
    mh.msg_controllen = 0;
    mh.msg_flags = 0;

    int res = ::sendmsg(m_iSocket, &mh, 0);
#else
    DWORD size = kPacketHeaderSize + packet.getLength();
    int res = ::WSASendTo(
        m_iSocket,
        (LPWSABUF) bufs, bufsCount, &size, 0,
        addr.get(), addr.size(),
        NULL, NULL);
    res = (0 == res) ? size : -1;
#endif

    // converting packet back into host order
    decodePacket(&packet);

    if (res < 0)
        return OsError();

    return success(res);
}

Result<int> UdpChannel::recvfrom(detail::SocketAddress& addr, CPacket& packet)
{
    assert(m_iSocket != INVALID_UDP_SOCKET);

    // Reserving size for any address.
    addr = detail::SocketAddress(AF_INET6);

    auto [bufs, bufsCount] = packet.ioBufs();

#ifndef _WIN32
    msghdr mh;
    mh.msg_name = addr.get();
    mh.msg_namelen = addr.size();
    mh.msg_iov = bufs;
    mh.msg_iovlen = bufsCount;
    mh.msg_control = NULL;
    mh.msg_controllen = 0;
    mh.msg_flags = 0;

#ifdef UNIX
    fd_set set;
    timeval tv;
    FD_ZERO(&set);
    FD_SET(m_iSocket, &set);
    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    ::select(m_iSocket + 1, &set, NULL, &set, &tv);
#endif

    int res = ::recvmsg(m_iSocket, &mh, 0);
#else
    DWORD size = kPacketHeaderSize + packet.getLength();
    DWORD flag = 0;

    int res = ::WSARecvFrom(
        m_iSocket,
        (LPWSABUF) bufs, bufsCount,
        &size, &flag,
        addr.get(), &addr.length(),
        NULL, NULL);
    res = (0 == res) ? size : -1;
#endif

    if (res <= 0)
    {
        packet.setLength(-1);
        return OsError();
    }

    packet.setLength(res - kPacketHeaderSize);

    // convert into local host order
    decodePacket(&packet);

    return success(packet.getLength());
}

Result<void> UdpChannel::shutdown()
{
    if (m_iSocket == INVALID_UDP_SOCKET)
        return success();

    int result = 0;
#ifdef _WIN32
    result = ::shutdown(m_iSocket, SD_BOTH);
#else
    result = ::shutdown(m_iSocket, SHUT_RDWR);
#endif

    if (result == 0)
        return success();

    return OsError();
}

void UdpChannel::closeSocket()
{
    if (m_iSocket == INVALID_UDP_SOCKET)
        return;

#ifndef _WIN32
    ::close(m_iSocket);
#else
    ::closesocket(m_iSocket);
#endif

    m_iSocket = INVALID_UDP_SOCKET;
}

template<typename Func>
void UdpChannel::convertHeader(CPacket* packet, Func func)
{
    auto p = packet->header();
    for (int j = 0; j < 4; ++j)
    {
        *p = func(*p);
        ++p;
    }
}

template<typename Func>
void UdpChannel::convertPayload(CPacket* packet, Func func)
{
    if (packet->getFlag() == PacketFlag::Control)
    {
        for (int i = 0, n = packet->getLength() / 4; i < n; ++i)
        {
            *((uint32_t*)packet->payload().data() + i) =
                func(*((uint32_t*)packet->payload().data() + i));
        }
    }
}

void UdpChannel::encodePacket(CPacket* packet)
{
    convertPayload(packet, [](auto val) { return htonl(val); });
    convertHeader(packet, [](auto val) { return htonl(val); });
}

void UdpChannel::decodePacket(CPacket* packet)
{
    convertHeader(packet, [](auto val) { return ntohl(val); });
    convertPayload(packet, [](auto val) { return ntohl(val); });
}
