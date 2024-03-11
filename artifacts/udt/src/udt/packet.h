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
Yunhong Gu, last updated 01/02/2011
*****************************************************************************/

#ifndef __UDT_PACKET_H__
#define __UDT_PACKET_H__

#include <array>
#include <iostream>
#include <memory>
#include <tuple>

#include "buffer.h"
#include "common.h"
#include "udt.h"

#ifdef _WIN32
struct IoBuf: public WSABUF
{
    char*& data() { return buf; };
    const char* data() const { return buf; };

    void setSize(std::size_t val) { len = val; }
    std::size_t size() const { return len; };
};

using iovec = WSABUF;
#else
struct IoBuf: public iovec
{
    char*& data() { return (char*&) iov_base; };
    const char* data() const { return (const char*&) iov_base; };

    void setSize(std::size_t val) { iov_len = val; }
    std::size_t size() const { return iov_len; };
};
#endif

template<std::size_t N>
class BufArray
{
public:
    std::size_t size() const
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            if (int(m_bufs[i].size()) < 0)
                return i;
        }

        return N;
    }

    IoBuf* bufs() { return m_bufs.data(); }
    const IoBuf* bufs() const { return m_bufs.data(); }

    const IoBuf& operator[](int i) const
    {
        return m_bufs[i];
    }

    IoBuf& operator[](int i)
    {
        return m_bufs[i];
    }

private:
    std::array<IoBuf, N> m_bufs;
};

//-------------------------------------------------------------------------------------------------

enum class ControlPacketType
{
    Handshake = 0,
    KeepAlive = 1,
    Acknowledgement = 2,
    LossReport = 3,
    DelayWarning = 4,
    Shutdown = 5,
    AcknowledgementOfAcknowledgement = 6,
    MsgDropRequest = 7,
    /** An error has happened to the peer side. */
    RemotePeerFailure = 8,
    /** 0x7FFF - reserved and user defined messages. */
    Reserved = 32767,
};

std::string to_string(ControlPacketType);

enum class PacketFlag
{
    Data = 0,
    Control = 1,
};

// The 128-bit header field.
using PacketHeader = uint32_t[4];

static constexpr int kPacketHeaderSize = sizeof(PacketHeader);

class UDT_API CPacket
{
private:
    PacketHeader m_nHeader;

public:
    int32_t& m_iSeqNo;                   // alias: sequence number
    int32_t& m_iMsgNo;                   // alias: message number
    int32_t& m_iTimeStamp;               // alias: timestamp
    int32_t& m_iID;                      // alias: socket ID

    const Buffer& payload() const { return m_payload; }
    Buffer& payload() { return m_payload; }

    void setPayload(Buffer buffer) { m_payload = std::move(buffer); }

public:
    CPacket();
    ~CPacket();

    CPacket(const CPacket&);
    CPacket(CPacket&&);

    // TODO: #akolesnikov Remove this. Replace usages with payload().size().
    int getLength() const;

    // TODO: #akolesnikov Remove this function. Replace usages with payload().resize() or
    // corresponding error reporting.
    void setLength(int len);

    // Functionality:
    //    Pack a Control packet.
    // Parameters:
    //    0) [in] pkttype: packet type filed.
    //    1) [in] lparam: pointer to the first data structure, explained by the packet type.
    //    2) [in] size: size of payload, in number of bytes;
    // NOTE: This function allocates the payload buffer, but it has to be filled
    // by the caller after this function returns.
    // Returned value:
    //    None.

    void pack(ControlPacketType pkttype, void* lparam = NULL, int payloadSize = 0);

    // Functionality:
    //    Read the packet flag.
    // Parameters:
    //    None.
    // Returned value:
    //    packet flag (0 or 1).

    PacketFlag getFlag() const;

    // Functionality:
    //    Read the packet type.
    // Parameters:
    //    None.
    // Returned value:
    //    packet type filed (000 ~ 111).

    ControlPacketType getType() const;

    // Functionality:
    //    Read the extended packet type.
    // Parameters:
    //    None.
    // Returned value:
    //    extended packet type filed (0x000 ~ 0xFFF).

    int getExtendedType() const;

    // Functionality:
    //    Read the ACK-2 seq. no.
    // Parameters:
    //    None.
    // Returned value:
    //    packet header field (bit 16~31).

    int32_t getAckSeqNo() const;

    // Functionality:
    //    Read the message boundary flag bit.
    // Parameters:
    //    None.
    // Returned value:
    //    packet header field [1] (bit 0~1).

    int getMsgBoundary() const;

    // Functionality:
    //    Read the message inorder delivery flag bit.
    // Parameters:
    //    None.
    // Returned value:
    //    packet header field [1] (bit 2).

    bool getMsgOrderFlag() const;

    // Functionality:
    //    Read the message sequence number.
    // Parameters:
    //    None.
    // Returned value:
    //    packet header field [1] (bit 3~31).

    int32_t getMsgSeq() const;

    // TODO: #akolesnikov Get rid of this function. Replace it with a copy constructor.
    std::unique_ptr<CPacket> clone() const;

    const uint32_t* header() const { return m_nHeader; }
    uint32_t* header() { return m_nHeader; }

    std::tuple<const IoBuf*, std::size_t> ioBufs() const;
    std::tuple<IoBuf*, std::size_t> ioBufs();

private:
    const int32_t __pad = 0;
    // TODO: #akolesnikov Remove mutable.
    mutable Buffer m_payload;
    // TODO: #akolesnikov Remove mutable.
    mutable BufArray<2> m_PacketVector;          // The 2-dimension vector of UDT packet [header, data]

    // TODO: #akolesnikov Remove const.
    void preparePacketVector() const;

    CPacket& operator=(const CPacket&) = delete;
    CPacket& operator=(CPacket&&) = delete;
};

////////////////////////////////////////////////////////////////////////////////

class CHandShake
{
public:
    CHandShake();

    int serialize(char* buf, int& size) const;
    [[nodiscard]] bool deserialize(const char* buf, int size);

public:
    static constexpr int m_iContentSize = 48;    // Size of hand shake data

public:
    int32_t m_iVersion = 0;          // UDT version
    int32_t m_iType = 0;             // UDT socket type
    int32_t m_iISN = 0;              // random initial sequence number
    int32_t m_iMSS = 0;              // maximum segment size
    int32_t m_iFlightFlagSize = 0;   // flow control window size
    int32_t m_iReqType = 0;          // connection request type: 1: regular connection request, 0: rendezvous connection request, -1/-2: response
    int32_t m_iID = 0;        // socket ID
    int32_t m_iCookie = 0;        // cookie
    uint32_t m_piPeerIP[4];    // The IP address that the peer's UDP port is bound to
};

// ------------------------------------------------------------------------------------------------

class CHandshakeValidator
{
public:
    bool validate(const CHandShake& hs) const;
};

#endif
