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
Yunhong Gu, last updated 02/12/2011
*****************************************************************************/


//////////////////////////////////////////////////////////////////////////////
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                        Packet Header                          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                                                               |
//   ~              Data / Control Information Field                 ~
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|                        Sequence Number                      |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |ff |o|                     Message Number                      |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          Time Stamp                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                     Destination Socket ID                     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   bit 0:
//      0: Data Packet
//      1: Control Packet
//   bit ff:
//      11: solo message packet
//      10: first packet of a message
//      01: last packet of a message
//   bit o:
//      0: in order delivery not required
//      1: in order delivery required
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |1|            Type             |             Reserved          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                       Additional Info                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          Time Stamp                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                     Destination Socket ID                     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   bit 1-15:
//      0: Protocol Connection Handshake
//              Add. Info:    Undefined
//              Control Info: Handshake information (see CHandShake)
//      1: Keep-alive
//              Add. Info:    Undefined
//              Control Info: None
//      2: Acknowledgement (ACK)
//              Add. Info:    The ACK sequence number
//              Control Info: The sequence number to which (but not include) all the previous packets have beed received
//              Optional:     RTT
//                            RTT Variance
//                            available receiver buffer size (in bytes)
//                            advertised flow window size (number of packets)
//                            estimated bandwidth (number of packets per second)
//      3: Negative Acknowledgement (NAK)
//              Add. Info:    Undefined
//              Control Info: Loss list (see loss list coding below)
//      4: Congestion/Delay Warning
//              Add. Info:    Undefined
//              Control Info: None
//      5: Shutdown
//              Add. Info:    Undefined
//              Control Info: None
//      6: Acknowledgement of Acknowledement (ACK-square)
//              Add. Info:    The ACK sequence number
//              Control Info: None
//      7: Message Drop Request
//              Add. Info:    Message ID
//              Control Info: first sequence number of the message
//                            last seqeunce number of the message
//      8: Error Signal from the Peer Side
//              Add. Info:    Error code
//              Control Info: None
//      0x7FFF: Explained by bits 16 - 31
//
//   bit 16 - 31:
//      This space is used for future expansion or user defined control packets.
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |1|                 Sequence Number a (first)                   |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|                 Sequence Number b (last)                    |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |0|                 Sequence Number (single)                    |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Loss List Field Coding:
//      For any consectutive lost seqeunce numbers that the differnece between
//      the last and first is more than 1, only record the first (a) and the
//      the last (b) sequence numbers in the loss list field, and modify the
//      the first bit of a to 1.
//      For any single loss or consectutive loss less than 2 packets, use
//      the original sequence numbers in the field.


#include <cstring>
#include "log.h"
#include "packet.h"

std::string to_string(ControlPacketType packetType)
{
    switch (packetType)
    {
        case ControlPacketType::Handshake:
            return "Handshake";
        case ControlPacketType::KeepAlive:
            return "KeepAlive";
        case ControlPacketType::Acknowledgement:
            return "Acknowledgement";
        case ControlPacketType::LossReport:
            return "LossReport";
        case ControlPacketType::DelayWarning:
            return "DelayWarning";
        case ControlPacketType::Shutdown:
            return "Shutdown";
        case ControlPacketType::AcknowledgementOfAcknowledgement:
            return "AcknowledgementOfAcknowledgement";
        case ControlPacketType::MsgDropRequest:
            return "MsgDropRequest";
        case ControlPacketType::RemotePeerFailure:
            return "RemotePeerFailure";
        default:
            return "Unknown " + std::to_string((int) packetType);
    }
}

//-------------------------------------------------------------------------------------------------

// Set up the aliases in the constructure
CPacket::CPacket():
    m_iSeqNo((int32_t&)(m_nHeader[0])),
    m_iMsgNo((int32_t&)(m_nHeader[1])),
    m_iTimeStamp((int32_t&)(m_nHeader[2])),
    m_iID((int32_t&)(m_nHeader[3]))
{
    memset(m_nHeader, 0, sizeof(m_nHeader));

    m_PacketVector[0].data() = (char*)m_nHeader;
    m_PacketVector[0].setSize(kPacketHeaderSize);
}

CPacket::~CPacket()
{
}

CPacket::CPacket(const CPacket& right):
    CPacket()
{
    memcpy(m_nHeader, right.m_nHeader, sizeof(m_nHeader));
    m_payload = right.m_payload;
}

CPacket::CPacket(CPacket&& right):
    CPacket()
{
    memcpy(m_nHeader, right.m_nHeader, sizeof(m_nHeader));
    m_payload = std::move(right.m_payload);
}

int CPacket::getLength() const
{
    return m_payload.size();
}

void CPacket::setLength(int len)
{
    // NOTE: Forbidding negative values.
    // 1. The underlying buffer size is unsigned.
    // 2. Error reporting must be done directly with error codes, not with buffer size.
    m_payload.resize(std::max(len, 0));
}

void CPacket::pack(ControlPacketType pkttype, void* lparam, int payloadSize)
{
    // Set (bit-0 = 1) and (bit-1~15 = type)
    m_nHeader[0] = 0x80000000 | (((int)pkttype) << 16);

    // Set additional information and control information field
    switch (pkttype)
    {
        case ControlPacketType::Acknowledgement: //0010 - Acknowledgement (ACK)
                                          // ACK packet seq. no.
            if (NULL != lparam)
                m_nHeader[1] = *(int32_t *)lparam;

            // data ACK seq. no.
            // optional: RTT (microsends), RTT variance (microseconds) advertised flow window size (packets), and estimated link capacity (packets per second)
            m_payload.resize(payloadSize);

            break;

        case ControlPacketType::AcknowledgementOfAcknowledgement: //0110 - Acknowledgement of Acknowledgement (ACK-2)
                                                           // ACK packet seq. no.
            m_nHeader[1] = *(int32_t *)lparam;

            // control info field should be none
            // but "writev" does not allow this
            m_payload.assign((char*) &__pad, 4);

            break;

        case ControlPacketType::LossReport: //0011 - Loss Report (NAK)
                                     // loss list
            m_payload.resize(payloadSize);

            break;

        case ControlPacketType::DelayWarning: //0100 - Congestion Warning
                                       // control info field should be none
                                       // but "writev" does not allow this
            m_payload.assign((char*)&__pad, 4);

            break;

        case ControlPacketType::KeepAlive: //0001 - Keep-alive
                                    // control info field should be none
                                    // but "writev" does not allow this
            m_payload.assign((char*)&__pad, 4);

            break;

        case ControlPacketType::Handshake: //0000 - Handshake
                                    // control info filed is handshake info
            m_payload.resize(payloadSize);

            break;

        case ControlPacketType::Shutdown: //0101 - Shutdown
                                   // control info field should be none
                                   // but "writev" does not allow this
            m_payload.assign((char*)&__pad, 4);

            break;

        case ControlPacketType::MsgDropRequest: //0111 - Message Drop Request
                                         // msg id
            m_nHeader[1] = *(int32_t *)lparam;

            //first seq no, last seq no
            m_payload.resize(payloadSize);

            break;

        case ControlPacketType::RemotePeerFailure: //1000 - Error Signal from the Peer Side
                                            // Error type
            m_nHeader[1] = *(int32_t *)lparam;

            // control info field should be none
            // but "writev" does not allow this
            m_payload.assign((char*)&__pad, 4);

            break;

        case ControlPacketType::Reserved: //0x7FFF - Reserved for user defined control packets
                                   // for extended control packet
                                   // "lparam" contains the extended type information for bit 16 - 31
            m_nHeader[0] |= *(int32_t *)lparam;

            if (payloadSize > 0)
                m_payload.resize(payloadSize);
            else
                m_payload.assign((char*)&__pad, 4);

            break;

        default:
            break;
    }
}

PacketFlag CPacket::getFlag() const
{
    // read bit 0
    return static_cast<PacketFlag>(m_nHeader[0] >> 31);
}

ControlPacketType CPacket::getType() const
{
    // read bit 1~15
    return static_cast<ControlPacketType>((m_nHeader[0] >> 16) & 0x00007FFF);
}

int CPacket::getExtendedType() const
{
    // read bit 16~31
    return m_nHeader[0] & 0x0000FFFF;
}

int32_t CPacket::getAckSeqNo() const
{
    // read additional information field
    return m_nHeader[1];
}

int CPacket::getMsgBoundary() const
{
    // read [1] bit 0~1
    return m_nHeader[1] >> 30;
}

bool CPacket::getMsgOrderFlag() const
{
    // read [1] bit 2
    return (1 == ((m_nHeader[1] >> 29) & 1));
}

int32_t CPacket::getMsgSeq() const
{
    // read [1] bit 3~31
    return m_nHeader[1] & 0x1FFFFFFF;
}

std::unique_ptr<CPacket> CPacket::clone() const
{
    auto pkt = std::make_unique<CPacket>();
    memcpy(pkt->m_nHeader, m_nHeader, kPacketHeaderSize);
    pkt->m_payload = m_payload;
    return pkt;
}

std::tuple<const IoBuf*, std::size_t> CPacket::ioBufs() const
{
    preparePacketVector();
    return std::make_tuple(m_PacketVector.bufs(), m_PacketVector.size());
}

std::tuple<IoBuf*, std::size_t> CPacket::ioBufs()
{
    preparePacketVector();
    return std::make_tuple(m_PacketVector.bufs(), m_PacketVector.size());
}

void CPacket::preparePacketVector() const
{
    m_PacketVector[1].data() = m_payload.data();
    m_PacketVector[1].setSize(m_payload.size());
}

//-------------------------------------------------------------------------------------------------

CHandShake::CHandShake()
{
    memset(m_piPeerIP, 0, sizeof(m_piPeerIP));
}

int CHandShake::serialize(char* buf, int& size) const
{
    if (size < m_iContentSize)
        return -1;

    int32_t* p = (int32_t*)buf;
    *p++ = m_iVersion;
    *p++ = m_iType;
    *p++ = m_iISN;
    *p++ = m_iMSS;
    *p++ = m_iFlightFlagSize;
    *p++ = m_iReqType;
    *p++ = m_iID;
    *p++ = m_iCookie;
    for (int i = 0; i < 4; ++i)
        *p++ = m_piPeerIP[i];

    size = m_iContentSize;

    return 0;
}

bool CHandShake::deserialize(const char* buf, int size)
{
    if (size < m_iContentSize)
        return false;

    int32_t* p = (int32_t*)buf;
    m_iVersion = *p++;
    m_iType = *p++;
    m_iISN = *p++;
    m_iMSS = *p++;
    m_iFlightFlagSize = *p++;
    m_iReqType = *p++;
    m_iID = *p++;
    m_iCookie = *p++;
    for (int i = 0; i < 4; ++i)
        m_piPeerIP[i] = *p++;

    return CHandshakeValidator{}.validate(*this);
}

bool CHandshakeValidator::validate(const CHandShake& hs) const
{
    UDT_LOG_DBG(
        "handshake: version: {}, type: {}, isn: {}, mss: {}, flightFlagSize: {}, reqType: {}, "
        "ID: {}, cookie: {}, peerIP: {}.{}.{}.{}",
        hs.m_iVersion, hs.m_iType, hs.m_iISN, hs.m_iMSS, hs.m_iFlightFlagSize, hs.m_iReqType, hs.m_iID,
        hs.m_iCookie, hs.m_piPeerIP[0], hs.m_piPeerIP[1], hs.m_piPeerIP[2], hs.m_piPeerIP[3]);

    if (hs.m_iMSS <= 0 || hs.m_iMSS > UDT::kMSSMax)
        return false;

    if (hs.m_iFlightFlagSize <= 0 || hs.m_iFlightFlagSize > UDT::kDefaultRecvWindowSize * 2)
        return false;

    if (hs.m_iVersion < 0 || hs.m_iVersion > UDT::kVersion)
        return false;

    if (hs.m_iType != 0 && hs.m_iType != UDTSockType::UDT_STREAM && hs.m_iType != UDTSockType::UDT_DGRAM)
        return false;

    return true;
}
