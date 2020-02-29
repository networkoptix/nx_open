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
*****************************************************************************/

/*****************************************************************************
written by
Yunhong Gu, last updated 01/27/2011
*****************************************************************************/

#ifndef __UDT_CHANNEL_H__
#define __UDT_CHANNEL_H__

#include <functional>
#include <optional>
#include <thread>

#include "udt.h"
#include "packet.h"
#include "result.h"
#include "socket_addresss.h"

class UDT_API AbstractUdpChannel
{
public:
    virtual ~AbstractUdpChannel() = default;

    virtual Result<> open(const std::optional<detail::SocketAddress>& localAddrToUse) = 0;
    virtual Result<> open(UDPSOCKET udpsock) = 0;
    virtual int getSndBufSize() = 0;
    virtual int getRcvBufSize() = 0;
    virtual void setSndBufSize(int size) = 0;
    virtual void setRcvBufSize(int size) = 0;
    virtual detail::SocketAddress getSockAddr() const = 0;

    // Functionality:
    //    Send a packet to the given address.
    // Parameters:
    //    0) [in] addr: pointer to the destination address.
    //    1) [in] packet: reference to a CPacket entity.
    // Returned value:
    //    Actual size of data sent.
    // TODO: #ak Should accept "const CPacket&".
    virtual Result<int> sendto(const detail::SocketAddress& addr, CPacket packet) = 0;

    // Functionality:
    //    Receive a packet from the channel and record the source address.
    // Parameters:
    //    0) [in] addr: pointer to the source address.
    //    1) [in] packet: reference to a CPacket entity.
    // Returned value:
    //    Actual size of data received.
    virtual Result<int> recvfrom(detail::SocketAddress& addr, CPacket& packet) = 0;

    virtual Result<> shutdown() = 0;
};

//-------------------------------------------------------------------------------------------------

class UDT_API UdpChannelFactory
{
public:
    using FactoryFunc = std::function<std::unique_ptr<AbstractUdpChannel>(int /*ipVersion*/)>;

    std::unique_ptr<AbstractUdpChannel> create(int ipVersion);

    /**
     * @return The previous function.
     */
    FactoryFunc setCustomFunc(FactoryFunc func);

    static UdpChannelFactory& instance();

private:
    FactoryFunc m_customFunc;
};

//-------------------------------------------------------------------------------------------------

class UDT_API UdpChannel:
    public AbstractUdpChannel
{
public:
    UdpChannel(int version);
    ~UdpChannel();

    virtual Result<> open(const std::optional<detail::SocketAddress>& localAddrToUse) override;
    virtual Result<> open(UDPSOCKET udpsock) override;

    virtual int getSndBufSize() override;
    virtual int getRcvBufSize() override;
    virtual void setSndBufSize(int size) override;
    virtual void setRcvBufSize(int size) override;
    virtual detail::SocketAddress getSockAddr() const override;
    virtual Result<int> sendto(const detail::SocketAddress& addr, CPacket packet) override;
    virtual Result<int> recvfrom(detail::SocketAddress& addr, CPacket& packet) override;
    virtual Result<> shutdown() override;

private:
    Result<> setUDPSockOpt();

private:
    int m_iIPversion;                    // IP version

    UDPSOCKET m_iSocket;                 // socket descriptor

    int m_iSndBufSize;                   // UDP sending buffer size
    int m_iRcvBufSize;                   // UDP receiving buffer size

                                         // Functionality:
                                         //    Disconnect and close the UDP entity.
                                         // Parameters:
                                         //    None.
                                         // Returned value:
                                         //    None.

    void closeSocket();

    template<typename Func>
    static void convertHeader(CPacket* packet, Func func);

    template<typename Func>
    static void convertPayload(CPacket* packet, Func func);

    static void encodePacket(CPacket* packet);
    static void decodePacket(CPacket* packet);
};

#endif
