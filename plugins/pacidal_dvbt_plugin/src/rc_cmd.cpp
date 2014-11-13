#include "rc_cmd.h"

namespace ite
{

unsigned Command::headCheck()
{
    if (! isOK())
        return ReturnChannelError_CMD_SYNTAX_ERROR;

    if (Cmd_checkChecksum(3, 12 + pktLength(), head()) != 0)
        return ReturnChannelError_CMD_CHECKSUM_ERROR;

    Word total_pktNum = totalPktNum();
    if (total_pktNum == 0)
        return ReturnChannelError_CMD_PKTCHECK_ERROR;

    Word pktNum = paketNum();
    if (pktNum == 0)
        return ReturnChannelError_CMD_PKTCHECK_ERROR;

    if (total_pktNum < pktNum)
        return ReturnChannelError_CMD_PKTCHECK_ERROR;

    return ModulatorError_NO_ERROR;
}

unsigned Command::deviceCheck(Device * device)
{
    if (device->hostRxDeviceID != rxDeviceID())
        return ReturnChannelError_CMD_RXDEVICEID_ERROR;

    if ((device->clientTxDeviceID != txDeviceID()) && ! isBroadcast())
        return ReturnChannelError_CMD_TXDEVICEID_ERROR;

    Word seqNum = sequenceNum();
    if (device->RCCmd_sequence_recv != (seqNum-1))
    {
        device->RCCmd_sequence_recv = seqNum;
        return ReturnChannelError_CMD_SEQUENCE_ERROR;
    }

    device->RCCmd_sequence_recv = seqNum;
    return ModulatorError_NO_ERROR;
}

unsigned RebuiltCmd::add(const Command& cmd)
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

bool RebuiltCmd::isChecksumOK() const
{
    unsigned index = 0;
    unsigned checkByte = 0;

    Cmd_CheckByteIndexRead(data(), index, &checkByte);
    return Cmd_checkChecksum(index, index + checkByte - 1, data()) == 0;
}

} // ret_chan
