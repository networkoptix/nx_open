#include "ret_chan/ret_chan_cmd.h"

#include "rc_command.h"

namespace ite
{

uint16_t RCCommand::in2outID(uint16_t cmdID)
{
    if (cmdID >= CMD_GetMetadataSettingsInput)
        return cmdID | 0xF000;
    return cmdID | 0x8000;
}

bool RCCommand::checkPacketCount()
{
    if (! totalPktNum())
        return false;
    if (! paketNum())
        return false;
    return paketNum() <= totalPktNum();
}

uint8_t RCCommand::checksum(const uint8_t * buffer, unsigned length)
{
    unsigned sum = 0;
    for (unsigned i = 0; i < length; ++i)
        sum += buffer[i];

    return sum & 0xff;
}

uint8_t RCCommand::calcChecksum() const
{
    unsigned length = HEAD_SIZE() + pktLength();
    if (length > m_size)
        length = m_size; // TODO

    // shift: '#', rxID
    return checksum(&m_data[3], length-3);
}

unsigned RebuiltCmd::add(const RCCommand& cmd)
{
    const Byte * buffer = cmd.content();
    unsigned short bufferSize = cmd.pktLength();
    unsigned short total_pktNum = cmd.totalPktNum();
    unsigned short pktNum = cmd.paketNum();

    ++packets_;

    if (bufferSize == 0)
        return ReturnChannelError::CMD_WRONG_LENGTH;

    if (pktNum > total_pktNum)
        return ReturnChannelError::CMD_PKTCHECK_ERROR;

    if (pktNum == 1)
    {
        clear();

        expectedPackets_ = total_pktNum;
        expectedSize_ = ((buffer[0]<<24) | (buffer[1]<<16) | (buffer[2]<<8) | buffer[3]) + 1;

        cmd_.reserve(expectedSize_);
    }

    if (total_pktNum != expectedPackets_)
        return ReturnChannelError::CMD_PKTCHECK_ERROR;

    if (cmd_.size() + bufferSize > expectedSize_)
        return ReturnChannelError::CMD_WRONG_LENGTH;

    for (unsigned i=0; i<bufferSize; ++i)
        cmd_.push_back( buffer[i] );

    if (total_pktNum == pktNum && cmd_.size() != expectedSize_)
        return ReturnChannelError::CMD_PKTCHECK_ERROR;

    return ReturnChannelError::NO_ERROR;
}

} // ret_chan
