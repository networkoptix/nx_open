#include "ret_chan/ret_chan.h"

#include "rc_command.h"
#include "tx_device.h"

namespace ite
{

bool RcPacket::checkPacketCount()
{
    if (! totalPktNum())
        return false;
    if (! paketNum())
        return false;
    return paketNum() <= totalPktNum();
}

uint8_t RcPacket::checksum(const uint8_t * buffer, unsigned length)
{
    unsigned sum = 0;
    for (unsigned i = 0; i < length; ++i)
        sum += buffer[i];

    return sum & 0xff;
}

uint8_t RcPacket::calcChecksum() const
{
    unsigned length = HEAD_SIZE() + pktLength();
    if (length > m_size)
        length = m_size; // TODO

    // shift: '#', rxID
    return checksum(&m_data[3], length-3);
}

void RcPacket::print() const
{
    if (! hasData())
    {
        debug_printf("[RC] packet error: no data\n");
        return;
    }

    if (! hasTags())
    {
        debug_printf("[RC] packet error: no tag\n");
        return;
    }

    if (! validLength())
    {
        debug_printf("[RC] packet error: wrong legth %d\n", pktLength());
        return;
    }

    if (! checksumOK())
    {
        debug_printf("[RC] packet error: wrong checksum\n");
        return;
    }

    debug_printf("[RC] rx: %d; tx: %d; total pktNum: %d; pktNum: %d; seqNum: %d; pktLength: %d\n",
           rxID(), txID(), totalPktNum(), paketNum(), sequenceNum(), pktLength());
}

//

uint16_t RcCommand::in2outID(uint16_t cmdID)
{
    if (cmdID >= CMD_GetMetadataSettingsInput)
        return cmdID | 0xF000;
    return cmdID | 0x8000;
}

bool RcCommand::addPacket(const RcPacket& pkt)
{
    if (! pkt.isOK())
        return false;

    uint16_t total_pktNum = pkt.totalPktNum();
    uint16_t pktNum = pkt.paketNum();
    if (pktNum > total_pktNum)
        return false;

    const uint8_t * buffer = pkt.content();
    uint16_t bufferSize = pkt.pktLength();
    if (buffer == nullptr || bufferSize == 0)
        return false;

    if (pktNum == 1)
    {
        clear();
        for (unsigned i = 0; i < bufferSize; ++i)
            m_buffer.push_back( buffer[i] );
    }
    else
    {
        if (size() < MIN_SIZE)
            return false;

        unsigned len = cmdLength() + 1;
        if (m_buffer.size() + bufferSize > len)
            return false;

        for (unsigned i = 0; i < bufferSize; ++i)
            m_buffer.push_back( buffer[i] );
    }

    if (total_pktNum == pktNum)
    {
        if (size() < MIN_SIZE)
            return false;

        unsigned len = cmdLength() + 1;
        if (size() > MIN_SIZE && size() > len)
            m_buffer.resize(len);

        if (! isValid())
            return false;
    }

    return true;
}

void RcCommand::mkPackets(SendSequence& sseq, uint16_t rxID, std::vector<RcPacketBuffer>& pkts) const
{
    if (! isValid())
        return;

#if 0
    if (sinfo.txID == 0xFFFF)
    {
        uint16_t tmpCmd = buffer[4]<<8 | buffer[5];

        if (! sinfo.tsMode || tmpCmd != CMD_GetTxDeviceAddressIDInput)
            return;
    }
#endif

    uint16_t total_pktCount = size() / RcPacket::RC_MAX_SIZE;
    if (size() % RcPacket::RC_MAX_SIZE)
        ++total_pktCount;

    pkts.resize(total_pktCount);

    const uint8_t * ptr = data();
    const uint8_t * pEnd = data() + size();
    for (unsigned i = 0; i < total_pktCount; ++i, ptr += RcPacket::RC_MAX_SIZE)
    {
        unsigned length = pEnd - ptr;
        if (length > RcPacket::RC_MAX_SIZE)
            length = RcPacket::RC_MAX_SIZE;

        RcPacket pkt = pkts[i].packet();

        ++sseq.rcSeq;
        pkt.setRxID(rxID);
        pkt.setTxID(sseq.txID);
        pkt.setTotalPktNum(total_pktCount);
        pkt.setPaketNum(i + 1);
        pkt.setSequenceNum(sseq.rcSeq);

        pkt.setPktLength(length);
        pkt.setContent(ptr, length);

        pkt.setChecksum();
        pkt.setTags();
    }
}

#if 0
void TxDevice::rc_sendTsPacket(uint8_t * tsPkt, unsigned length)
{
    Word pid = RETURN_CHANNEL_PID;
    Byte seqNum = ++m_seq.ts_send;

    //TS header
    tsPkt[0] = 0x47;
    tsPkt[1] = 0x20 | ((uint8_t) (pid >> 8));       // TEI + .. + PID
    tsPkt[2] = (uint8_t) pid;                       // PID
    tsPkt[3] = 0x10| ((uint8_t) (seqNum & 0x0F));   // ... + continuity counter

    //User_returnChannelBusTx(length, tsPkt, &txLen);
}
#endif

} // ret_chan
