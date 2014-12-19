#include <sys/time.h>

#include "rc_device_info.h"

namespace ite
{
    DeviceInfo::DeviceInfo(unsigned short rxID, unsigned short txID)
    :   waitingCmd_(CMD_GetTxDeviceAddressIDOutput),
        waitingResponse_(false),
        isActive_(false)
    {
        //RC_Init_RCHeadInfo(&info_.device, rxID, txID, 0xFFFF);
        info_.device.clientTxDeviceID = txID;
        info_.device.hostRxDeviceID = rxID;
        info_.device.RCCmd_sequence = 0;
        info_.device.RCCmd_sequence_recv = 0xFFFF;

        //RC_Init_TSHeadInfo(&info_.device, RETURN_CHANNEL_PID, 0, False);
        info_.device.PID = RETURN_CHANNEL_PID;
        info_.device.TS_sequence = 0;
        info_.device.TS_sequence_recv = 0;
        info_.device.TSMode = False;

        //RC_Init_CmdSendConfig(&info_.cmdSendConfig);
        info_.cmdSendConfig.bIsRepeatPacket = False;
        info_.cmdSendConfig.byRepeatPacketNumber = 1;
        info_.cmdSendConfig.byRepeatPacketTimeInterval = 0;
        info_.cmdSendConfig.bIsCmdBroadcast = False;

        User_Host_init(&info_);
        User_askUserSecurity(&info_.security);
    }

    DeviceInfo::~DeviceInfo()
    {
        User_Host_uninit(&info_);
    }

    RCError DeviceInfo::parseCommand()
    {
        if (! cmd().isOK())
            return RCERR_WRONG_CMD;

        if (cmd().size() < 8)
            return RCERR_WRONG_LENGTH;

        if (cmd().returnCode() != 0) // TODO: not all cammands
            return RCERR_RET_CODE;

        unsigned short commandID = cmd().commandID();
        unsigned error = Cmd_HostParser(&info_, commandID, cmd().data(), cmd().size());

        if (isWaiting() && waitingCmd() == commandID)
            setWaiting(false);

        if (error == ModulatorError_NO_ERROR)
        {
            cmd().setValid();
            return RCERR_NO_ERROR;
        }
        else if (error == ReturnChannelError_Reply_WRONG_LENGTH)
            return RCERR_WRONG_LENGTH;
        else if (error == ReturnChannelError_CMD_NOT_SUPPORTED)
            return RCERR_WRONG_CMD;

        return RCERR_NOT_PARSED;
    }

    bool DeviceInfo::wait(int iWaitTime)
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

    bool DeviceInfo::setChannel(unsigned channel)
    {
        unsigned freq = DeviceInfo::chanFrequency(channel);

        if (info_.transmissionParameterCapabilities.frequencyMin > 0 &&
            freq < info_.transmissionParameterCapabilities.frequencyMin)
            return false;

        if (info_.transmissionParameterCapabilities.frequencyMax > 0 &&
            freq > info_.transmissionParameterCapabilities.frequencyMax)
            return false;

        info_.transmissionParameter.frequency = freq;

        setTransmissionParameters();
        return true;
    }

    bool DeviceInfo::setVideoEncConfig(unsigned streamNo, const TxVideoEncConfig& conf)
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

    void DeviceInfo::print() const
    {
        printf("TX ID: %04x RX ID: %04x\n", info_.device.clientTxDeviceID, info_.device.hostRxDeviceID);
        printf("Frequency in KHz (ex. 666000): %u\n", info_.transmissionParameter.frequency);
        printf("Bandwidth in MHz (ex. 1~8): %d\n", info_.transmissionParameter.bandwidth);
    #if 0
        printf("Constellation (0: QPSK, 1: 16QAM, 2: 64QAM): %d\n", info_.transmissionParameter.constellation);
        printf("Transmission Mode (0: 2K, 1: 8K 2: 4K): %d\n", info_.transmissionParameter.FFT);
        printf("Code Rate (0: 1/2, 1: 2/3, 2: 3/4, 3:5/6, 4: 7/8): %d\n", info_.transmissionParameter.codeRate);
        printf("Guard Interval (0: 1/32, 1: 1/16, 2: 1/8, 3: 1/4): %d\n", info_.transmissionParameter.interval);
        printf("Attenuation (ex: -100~100): %d\n", info_.transmissionParameter.attenuation_signed);
        printf("TPS Cell ID : %d\n", info_.transmissionParameter.TPSCellID);
        printf("Channel Number (ex: 0~255): %d\n", info_.transmissionParameter.channelNum);
        printf("Bandwidth Strapping (ex: 0:7+8MHz, 1:6MHz, 2:7MHz, 3:8MHz): %d\n", info_.transmissionParameter.bandwidthStrapping);
        printf("TV Standard (ex: 0: DVB-T 1: ISDB-T): %d\n", info_.transmissionParameter.TVStandard);
        printf("Segmentation Mode (ex: 0: ISDB-T Full segmentation mode 1: ISDB-T 1+12 segmentation mode): %d\n", info_.transmissionParameter.segmentationMode);
        printf("One-Seg Constellation (ex: 0: QPSK, 1: 16QAM, 2: 64QAM ): %d\n", info_.transmissionParameter.oneSeg_Constellation);
        printf("One-Seg Code Rate (ex: 0: 1/2, 1: 2/3, 2: 3/4, 3: 5/6, 4: 7/8 ): %d\n", info_.transmissionParameter.oneSeg_CodeRate);
    #endif
    }
}
