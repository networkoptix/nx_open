/*****************************************************************************
Copyright (c) 2001 - 2009, The Board of Trustees of the University of Illinois.
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
Yunhong Gu, last updated 02/28/2012
*****************************************************************************/


#ifndef __UDT_CCC_H__
#define __UDT_CCC_H__

#include <string>

#include "udt.h"
#include "packet.h"


class UDT_API CCC
{
    friend class CUDT;

public:
    CCC();
    virtual ~CCC();

    // Functionality:
    //    Callback function to be called (only) at the start of a UDT connection.
    //    note that this is different from CCC(), which is always called.
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    virtual void init() {}

    // Functionality:
    //    Callback function to be called when a UDT connection is closed.
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    virtual void close() {}

    // Functionality:
    //    Callback function to be called when an ACK packet is received.
    // Parameters:
    //    0) [in] ackno: the data sequence number acknowledged by this ACK.
    // Returned value:
    //    None.

    virtual void onACK(int32_t) {}

    // Functionality:
    //    Callback function to be called when a loss report is received.
    // Parameters:
    //    0) [in] losslist: list of sequence number of packets, in the format describled in packet.cpp.
    //    1) [in] size: length of the loss list.
    // Returned value:
    //    None.

    virtual void onLoss(const int32_t*, int) {}

    // Functionality:
    //    Callback function to be called when a timeout event occurs.
    // Parameters:
    //    None.
    // Returned value:
    //    None.

    virtual void onTimeout() {}

    // Functionality:
    //    Callback function to be called when a data is sent.
    // Parameters:
    //    0) [in] seqno: the data sequence number.
    //    1) [in] size: the payload size.
    // Returned value:
    //    None.

    virtual void onPktSent(const CPacket*) {}

    // Functionality:
    //    Callback function to be called when a data is received.
    // Parameters:
    //    0) [in] seqno: the data sequence number.
    //    1) [in] size: the payload size.
    // Returned value:
    //    None.

    virtual void onPktReceived(const CPacket*) {}

    // Functionality:
    //    Callback function to Process a user defined packet.
    // Parameters:
    //    0) [in] pkt: the user defined packet.
    // Returned value:
    //    None.

    virtual void processCustomMsg(const CPacket*) {}

protected:

    // Functionality:
    //    Set periodical acknowldging and the ACK period.
    // Parameters:
    //    0) [in] msINT: the period to send an ACK.
    // Returned value:
    //    None.

    void setACKTimer(std::chrono::microseconds msINT);

    // Functionality:
    //    Set packet-based acknowldging and the number of packets to send an ACK.
    // Parameters:
    //    0) [in] pktINT: the number of packets to send an ACK.
    // Returned value:
    //    None.

    void setACKInterval(int pktINT);

    // Functionality:
    //    Set RTO value.
    // Returned value:
    //    None.

    void setRTO(std::chrono::microseconds usRTO);

    // Functionality:
    //    retrieve performance information.
    // Parameters:
    //    None.
    // Returned value:
    //    Pointer to a performance info structure.

    const CPerfMon* getPerfInfo();

    // Functionality:
    //    Set user defined parameters.
    // Returned value:
    //    None.

    void setUserParam(std::string val);

private:
    void setMSS(int mss);
    void setMaxCWndSize(int cwnd);
    void setBandwidth(int bw);
    void setSndCurrSeqNo(int32_t seqno);
    void setRcvRate(int rcvrate);
    void setRTT(std::chrono::microseconds rtt);

protected:
    std::chrono::microseconds m_iSYNInterval;    // UDT constant parameter, SYN

    double m_dPktSndPeriod = 1.0;              // Packet sending period, in microseconds
    double m_dCWndSize = 16.0;                  // Congestion window size, in packets

    int m_iBandwidth = 0;            // estimated bandwidth, packets per second
    double m_dMaxCWndSize = 0.0;               // maximum cwnd size, in packets

    int m_iMSS = 0;                // Maximum Packet Size, including all packet headers
    int32_t m_iSndCurrSeqNo = 0;        // current maximum seq no sent out
    int m_iRcvRate = 0;            // packet arrive rate at receiver side, packets per second
    std::chrono::microseconds m_iRTT = std::chrono::microseconds::zero();   // current estimated RTT, microsecond

    std::string m_pcParam;            // user defined parameter

private:
    UDTSOCKET m_UDT = 0;                     // The UDT entity that this congestion control algorithm is bound to

    std::chrono::microseconds m_iACKPeriod = std::chrono::microseconds::zero();                    // Periodical timer to send an ACK
    int m_iACKInterval = 0;                  // How many packets to send one ACK, in packets

    bool m_bUserDefinedRTO = false;              // if the RTO value is defined by users
    std::chrono::microseconds m_iRTO = std::chrono::microseconds(-1);   // RTO value, microseconds

    CPerfMon m_PerfInfo;                 // protocol statistics information
};

class CCCVirtualFactory
{
public:
    virtual ~CCCVirtualFactory() {}

    virtual std::unique_ptr<CCC> create() = 0;
    virtual std::unique_ptr<CCCVirtualFactory> clone() = 0;
};

template <class T>
class CCCFactory: public CCCVirtualFactory
{
public:
    virtual ~CCCFactory() {}

    virtual std::unique_ptr<CCC> create() { return std::make_unique<T>(); }
    virtual std::unique_ptr<CCCVirtualFactory> clone() { return std::make_unique<CCCFactory<T>>(); }
};

class CUDTCC: public CCC
{
public:
    CUDTCC() = default;

    virtual void init();
    virtual void onACK(int32_t);
    virtual void onLoss(const int32_t*, int);
    virtual void onTimeout();

private:
    std::chrono::microseconds m_iRCInterval = std::chrono::microseconds::zero();            // UDT Rate control interval
    std::chrono::microseconds m_LastRCTime = std::chrono::microseconds::zero();        // last rate increase time
    bool m_bSlowStart = false;            // if in slow start phase
    int32_t m_iLastAck = 0;            // last ACKed seq no
    bool m_bLoss = false;            // if loss happened since last rate increase
    int32_t m_iLastDecSeq = 0;        // max pkt seq no sent out when last decrease happened
    double m_dLastDecPeriod = 0.0;        // value of pktsndperiod when last decrease happened
    int m_iNAKCount = 0;                     // NAK counter
    int m_iDecRandom = 0;                    // random threshold on decrease by number of loss events
    int m_iAvgNAKNum = 0;                    // average number of NAKs per congestion
    int m_iDecCount = 0;            // number of decreases in a congestion epoch
};

#endif
