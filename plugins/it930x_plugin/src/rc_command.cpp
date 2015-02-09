#include "rc_command.h"

namespace ite
{

bool RCCommand::checkPacketCount()
{
    if (! totalPktNum())
        return false;
    if (! paketNum())
        return false;
    return paketNum() <= totalPktNum();
}

bool RCCommand::checksum() const
{
    // -2 bytes rxID, -1 byte checksum byte
    unsigned sum = 0;
    for (unsigned i = 3; i < buffer_.size() && i < headSize() + pktLength(); ++i)
        sum += buffer_[i];
    sum &= 0xff;

    Byte value = checkSumValue();
    return (sum == value);
}

unsigned RebuiltCmd::add(const RCCommand& cmd)
{
    const Byte * buffer = cmd.data();
    unsigned short bufferSize = cmd.pktLength();
    unsigned short total_pktNum = cmd.totalPktNum();
    unsigned short pktNum = cmd.paketNum();

    ++packets_;

    if (bufferSize == 0)
        return ReturnChannelError_CMD_WRONG_LENGTH;

    if (pktNum > total_pktNum)
        return ReturnChannelError_CMD_PKTCHECK_ERROR;

    if (pktNum == 1)
    {
        clear();

        expectedPackets_ = total_pktNum;
        expectedSize_ = ((buffer[0]<<24) | (buffer[1]<<16) | (buffer[2]<<8) | buffer[3]) + 1;

        cmd_.reserve(expectedSize_);
    }

    if (total_pktNum != expectedPackets_)
        return ReturnChannelError_CMD_PKTCHECK_ERROR;

    if (cmd_.size() + bufferSize > expectedSize_)
        return ReturnChannelError_CMD_WRONG_LENGTH;

    for (unsigned i=0; i<bufferSize; ++i)
        cmd_.push_back( buffer[i] );

    if (total_pktNum == pktNum && cmd_.size() != expectedSize_)
        return ReturnChannelError_CMD_PKTCHECK_ERROR;

    return ModulatorError_NO_ERROR;
}

} // ret_chan
