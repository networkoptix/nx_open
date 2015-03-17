#include <sys/time.h>

#include "ret_chan/ret_chan_user_host.h"
#include "ret_chan/ret_chan_user.h"

#include "tx_device.h"

// TODO
unsigned User_returnChannelBusTx(IN unsigned bufferLength, IN const Byte * buffer, OUT int * txLen)
{
    return 0;
}

unsigned User_returnChannelBusRx(IN unsigned bufferLength, OUT Byte * buffer, OUT int * rxLen)
{
    return 0;
}

// ret_chan/ret_chan_cmd_host.cpp
unsigned sendRC(ite::TxDevice * tx, unsigned short command);
unsigned parseRC(ite::TxDevice * tx, unsigned short command, const Byte * Buffer, unsigned bufferLength);


namespace ite
{
    TxDevice::TxDevice(unsigned short txID, unsigned freq)
    :   waitingCmd_(CMD_GetTxDeviceAddressIDOutput),
        waitingResponse_(false)
    {
        //RC_Init_RCHeadInfo(&info_.device, rxID, txID, 0xFFFF);
        device.clientTxDeviceID = txID;
        device.hostRxDeviceID = 0xFFFF;
        device.RCCmd_sequence = 0;
        device.RCCmd_sequence_recv = 0xFFFF;

        //RC_Init_TSHeadInfo(&info_.device, RETURN_CHANNEL_PID, 0, False);
        device.PID = RETURN_CHANNEL_PID;
        device.TS_sequence = 0;
        device.TS_sequence_recv = 0;
        device.TSMode = False;

        User_Host_init(this);
        User_askUserSecurity(&security);

        setFrequency(freq);
    }

    TxDevice::~TxDevice()
    {
        User_Host_uninit(this);
    }

    RCError TxDevice::parseCommand()
    {
        if (! cmd().isOK())
            return RCERR_WRONG_CMD;

        if (cmd().size() < 8)
            return RCERR_WRONG_LENGTH;

        if (cmd().returnCode() != 0) // TODO: not all cammands
            return RCERR_RET_CODE;

        unsigned short commandID = cmd().commandID();
        unsigned error = ::parseRC(this, commandID, cmd().data(), cmd().size());

        if (isWaiting() && waitingCmd() == commandID)
            setWaiting(false);

        if (error == ReturnChannelError::NO_ERROR)
        {
            cmd().setValid();
            return RCERR_NO_ERROR;
        }
        else if (error == ReturnChannelError::Reply_WRONG_LENGTH)
            return RCERR_WRONG_LENGTH;
        else if (error == ReturnChannelError::CMD_NOT_SUPPORTED)
            return RCERR_WRONG_CMD;

        return RCERR_NOT_PARSED;
    }

    bool TxDevice::wait(int iWaitTime)
    {
        struct timeval startTime;
        gettimeofday(&startTime, NULL);

        while (isWaiting())
        {
            usleep(SLEEP_TIME_MS * 1000);

            struct timeval curTime;
            gettimeofday(&curTime, NULL);

            int diffTime = (curTime.tv_usec + 1000000 * curTime.tv_sec) - (startTime.tv_usec + 1000000 * startTime.tv_sec);
            diffTime /= 1000; // usec -> msec
            if (diffTime > iWaitTime)
                return false;
        }

        return true;
    }

    bool TxDevice::setChannel(unsigned channel)
    {
        unsigned freq = freq4chan(channel);
#if 0
        if (transmissionParameterCapabilities.frequencyMin > 0 &&
            freq < transmissionParameterCapabilities.frequencyMin)
            return false;

        if (transmissionParameterCapabilities.frequencyMax > 0 &&
            freq > transmissionParameterCapabilities.frequencyMax)
            return false;

        transmissionParameter.frequency = freq;

        //setTransmissionParameters();
        return true;
#endif
        return false;
    }


#if 0
    bool TxDevice::setVideoEncConfig(unsigned streamNo, const TxVideoEncConfig& conf)
    {
#if 0
        getVideoEncoderConfigurations();
        if (! wait())
            return false;
#endif
        if (info_.videoEncConfig.configListSize < streamNo)
            return false;

        Cmd_StringClear(&info_.videoEncConfigSetParam.name);
        Cmd_StringClear(&info_.videoEncConfigSetParam.token);

        Cmd_StringSet(info_.videoEncConfig.configList[streamNo].name.stringData,
                      info_.videoEncConfig.configList[streamNo].name.stringLength,
                      &info_.videoEncConfigSetParam.name);
        Cmd_StringSet(info_.videoEncConfig.configList[streamNo].token.stringData,
                      info_.videoEncConfig.configList[streamNo].token.stringLength,
                      &info_.videoEncConfigSetParam.token);

        info_.videoEncConfigSetParam.useCount           = info_.videoEncConfig.configList[streamNo].useCount;
        info_.videoEncConfigSetParam.encoding           = info_.videoEncConfig.configList[streamNo].encoding;
        info_.videoEncConfigSetParam.width              = info_.videoEncConfig.configList[streamNo].width;
        info_.videoEncConfigSetParam.height             = info_.videoEncConfig.configList[streamNo].height;
        info_.videoEncConfigSetParam.quality            = info_.videoEncConfig.configList[streamNo].quality;
        info_.videoEncConfigSetParam.frameRateLimit     = info_.videoEncConfig.configList[streamNo].frameRateLimit;
        info_.videoEncConfigSetParam.encodingInterval   = info_.videoEncConfig.configList[streamNo].encodingInterval;
        info_.videoEncConfigSetParam.bitrateLimit       = info_.videoEncConfig.configList[streamNo].bitrateLimit;
        info_.videoEncConfigSetParam.rateControlType    = info_.videoEncConfig.configList[streamNo].rateControlType;
        info_.videoEncConfigSetParam.govLength          = info_.videoEncConfig.configList[streamNo].govLength;
        info_.videoEncConfigSetParam.profile            = info_.videoEncConfig.configList[streamNo].profile;
        info_.videoEncConfigSetParam.extensionFlag      = info_.videoEncConfig.configList[streamNo].extensionFlag;
        info_.videoEncConfigSetParam.targetBitrateLimit = info_.videoEncConfig.configList[streamNo].targetBitrateLimit;
        info_.videoEncConfigSetParam.aspectRatio        = info_.videoEncConfig.configList[streamNo].aspectRatio;
        info_.videoEncConfigSetParam.forcePersistence   = info_.videoEncConfig.configList[streamNo].forcePersistence;

        if (conf.width)
            info_.videoEncConfigSetParam.width = conf.width;
        if (conf.height)
            info_.videoEncConfigSetParam.height = conf.height;
        if (conf.bitrateLimit)
            info_.videoEncConfigSetParam.bitrateLimit = conf.bitrateLimit;
        if (conf.frameRateLimit)
            info_.videoEncConfigSetParam.frameRateLimit = conf.frameRateLimit;

        setVideoEncoderConfiguration();
        if (! wait())
            return false;
        return true;
    }
#endif

    void TxDevice::print() const
    {
        printf("TX ID: %04x RX ID: %04x\n", device.clientTxDeviceID, device.hostRxDeviceID);
        printf("Frequency in KHz (ex. 666000): %u\n", transmissionParameter.frequency);
        printf("Bandwidth in MHz (ex. 1~8): %d\n", transmissionParameter.bandwidth);
#if 0
        printf("Constellation (0: QPSK, 1: 16QAM, 2: 64QAM): %d\n", transmissionParameter.constellation);
        printf("Transmission Mode (0: 2K, 1: 8K 2: 4K): %d\n", transmissionParameter.FFT);
        printf("Code Rate (0: 1/2, 1: 2/3, 2: 3/4, 3:5/6, 4: 7/8): %d\n", transmissionParameter.codeRate);
        printf("Guard Interval (0: 1/32, 1: 1/16, 2: 1/8, 3: 1/4): %d\n", transmissionParameter.interval);
        printf("Attenuation (ex: -100~100): %d\n", transmissionParameter.attenuation_signed);
        printf("TPS Cell ID : %d\n", transmissionParameter.TPSCellID);
        printf("Channel Number (ex: 0~255): %d\n", transmissionParameter.channelNum);
        printf("Bandwidth Strapping (ex: 0:7+8MHz, 1:6MHz, 2:7MHz, 3:8MHz): %d\n", transmissionParameter.bandwidthStrapping);
        printf("TV Standard (ex: 0: DVB-T 1: ISDB-T): %d\n", transmissionParameter.TVStandard);
        printf("Segmentation Mode (ex: 0: ISDB-T Full segmentation mode 1: ISDB-T 1+12 segmentation mode): %d\n", transmissionParameter.segmentationMode);
        printf("One-Seg Constellation (ex: 0: QPSK, 1: 16QAM, 2: 64QAM ): %d\n", transmissionParameter.oneSeg_Constellation);
        printf("One-Seg Code Rate (ex: 0: 1/2, 1: 2/3, 2: 3/4, 3: 5/6, 4: 7/8 ): %d\n", transmissionParameter.oneSeg_CodeRate);
#endif
    }

    //

    void TxDevice::sendRC(uint16_t command)
    {
        //setWaiting(true, input2output(command));

        unsigned error = ::sendRC(this, command); // --> ret_chan
        if (error)
            throw "Can't send cmd";
    }

    unsigned TxDevice::rc_splitBuffer(Byte * buf, unsigned length, Byte * splitBuf, unsigned splitLength, unsigned * start)
    {
        unsigned offset = (*start);
        User_memory_set(splitBuf, 0, splitLength);

        if (length-offset > splitLength)
        {
            User_memory_copy(splitBuf, buf + offset, splitLength);

            offset = offset + splitLength;
            (*start) = offset;

            return (length-offset);
        }
        else
        {
            User_memory_copy(splitBuf, buf + offset, length - offset);

            (*start)  = length;
            return 0;
        }
    }

    void TxDevice::rc_addRC(Byte * srcbuf,  Byte * dstbuf, unsigned total_pktNum, unsigned pktNum, unsigned pktLength)
    {
        device.RCCmd_sequence++;
        if (device.bIsDebug)
            printf("RCCmd_sequence = %u\n", device.RCCmd_sequence);

        Word rxID = device.hostRxDeviceID;
        Word txID = device.clientTxDeviceID;
        Word seqNum = device.RCCmd_sequence;

        //RC header
        Byte index = 0;
        dstbuf[index + 183] = 0x0d;
        dstbuf[index++] = '#';                          // Leading Tag
        dstbuf[index++] = (Byte)(rxID>>8);
        dstbuf[index++] = (Byte)rxID;

        unsigned payload_start_point = index;
        dstbuf[index++] = (Byte)(txID>>8);
        dstbuf[index++] = (Byte)txID;
        dstbuf[index++] = (Byte)(total_pktNum>>8);      // total_pktNum
        dstbuf[index++] = (Byte)total_pktNum;           // total_pktNum
        dstbuf[index++] = (Byte)(pktNum>>8);            // pktNum
        dstbuf[index++] = (Byte)pktNum;                 // pktNum
        dstbuf[index++] = (Byte)(seqNum>>8);            // seqNum
        dstbuf[index++] = (Byte)seqNum;                 // seqNum
        dstbuf[index++] = (Byte)(pktLength>>8);         // pktLength
        dstbuf[index++] = (Byte)pktLength;              // pktLength

        User_memory_copy(dstbuf + index, srcbuf, pktLength);
        index = index + (Byte)pktLength;

        dstbuf[index] = checksum(&dstbuf[payload_start_point], index - payload_start_point);
        //++index;
    }

    void TxDevice::rc_addTS(Byte * dstbuf)
    {
        device.TS_sequence++;

        Word PID = device.PID;
        Byte seqNum = device.TS_sequence;

        //TS header
        dstbuf[0] = 0x47;                               // svnc_byte
        dstbuf[1] = 0x20 | ((Byte) (PID >> 8));         // TEI + .. +PID
        dstbuf[2] = (Byte) PID;                         // PID
        dstbuf[3] = 0x10| ((Byte) (seqNum & 0x0F));     // ... + continuity counter
    }

    unsigned TxDevice::rc_sendTSCmd(Byte * buffer, unsigned bufferSize)
    {
        Word tmpcommand = buffer[4]<<8 | buffer[5];

        if (device.clientTxDeviceID == 0xFFFF)
        {
            switch (tmpcommand)
            {
                case CMD_GetTxDeviceAddressIDInput:
                    break;
                default:
                    return ReturnChannelError::CMD_TXDEVICEID_ERROR;
            }
        }

        unsigned dwRemainder = bufferSize % RCMaxSize;
        unsigned total_pktCount = bufferSize / RCMaxSize;
        if (dwRemainder)
            ++total_pktCount;

        unsigned error = ReturnChannelError::NO_ERROR;
        Byte tsPkt[188] = {0xFF};
        Byte cmdData[RCMaxSize] = {0xFF};
        unsigned start = 0;

        //User_mutex_lock();

        unsigned pktCount = 1;
        for (; rc_splitBuffer(buffer, bufferSize, cmdData, RCMaxSize, &start); ++pktCount)
        {
            rc_addTS(tsPkt);
            rc_addRC(cmdData, &tsPkt[4], total_pktCount, pktCount, RCMaxSize);

            int txLen;
            error = User_returnChannelBusTx(sizeof(tsPkt), tsPkt, &txLen); // FIXME
            if (error)
                goto exit;
        }

        rc_addTS(tsPkt);

        if (dwRemainder == 0)
            rc_addRC(cmdData, &tsPkt[4], total_pktCount, pktCount, RCMaxSize);
        else
            rc_addRC(cmdData, &tsPkt[4], total_pktCount, pktCount, dwRemainder);

        int txLen;
        error = User_returnChannelBusTx(sizeof(tsPkt), tsPkt, &txLen); // FIXME
        if (error)
            goto exit;

    exit:
        //User_mutex_unlock();
        return error;
    }

    unsigned TxDevice::rc_sendBuffer(Byte * buffer, unsigned bufferSize)
    {
        if (bufferSize <= 0)
            return ReturnChannelError::CMD_WRONG_LENGTH;

        if (device.TSMode)
            return rc_sendTSCmd(buffer, bufferSize);

        if (device.clientTxDeviceID == 0xFFFF)
            return ReturnChannelError::CMD_TXDEVICEID_ERROR;

        //Word tmpcommand = buffer[4]<<8 | buffer[5];
        unsigned dwRemainder = bufferSize % RCMaxSize;
        unsigned total_pktCount = bufferSize / RCMaxSize;
        if (dwRemainder)
            ++total_pktCount;

        Byte cmdData[RCMaxSize] = {0xFF};
        Byte rcPkt[184] = {0xFF};
        unsigned start = 0;
        unsigned error = ReturnChannelError::NO_ERROR;

        //User_mutex_lock(); // LOCK

        unsigned pktCount = 1;
        for (; rc_splitBuffer(buffer, bufferSize, cmdData, RCMaxSize, &start); ++pktCount)
        {
            rc_addRC(cmdData, rcPkt, total_pktCount, pktCount, RCMaxSize);

            int txLen;
            error = User_returnChannelBusTx(sizeof(rcPkt), rcPkt, &txLen); // FIXME
            if (error)
                goto exit;
        }

        if (dwRemainder == 0)
            rc_addRC(cmdData, rcPkt, total_pktCount, pktCount, RCMaxSize);
        else
            rc_addRC(cmdData, rcPkt, total_pktCount, pktCount, dwRemainder);

        int txLen;
        error = User_returnChannelBusTx(sizeof(rcPkt), rcPkt, &txLen); // FIXME
        if (error)
            goto exit;

    exit:
        //User_mutex_unlock(); // UNLOCK
        return error;
    }
}
