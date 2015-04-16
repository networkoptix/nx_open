#include <ctime>

#include "tx_device.h"

// ret_chan/ret_chan_cmd_host.cpp
unsigned makeRcCommand(ite::TxDevice * tx, unsigned short command);
unsigned parseRC(ite::TxDevice * tx, unsigned short cmdID, const Byte * Buffer, unsigned bufferLength);

static void User_askUserSecurity(Security * security)
{
    Byte userName[20] = "userName";
    Byte password[20] = "password";

    security->userName.set(userName, 20);
    security->password.set(password, 20);
}

namespace ite
{
    TxDevice::TxDevice(unsigned short txID, unsigned freq)
    :   m_ready(false),
        m_wantedCmd(0),
        m_sequence(txID)
    {
        User_askUserSecurity(&security);

        setFrequency(freq);

        m_recvSecurity.userName.stringData = nullptr;
        m_recvSecurity.userName.stringLength = 0;
        m_recvSecurity.password.stringData = nullptr;
        m_recvSecurity.password.stringLength = 0;
    }

    void TxDevice::parse(RcPacket& pkt)
    {
        if (! pkt.isOK())
            return;

        if (m_cmdRecv.addPacket(pkt) && m_cmdRecv.isValid())
        {
            uint16_t cmdID = m_cmdRecv.commandID();
            uint8_t code = m_cmdRecv.returnCode();
            unsigned error = ReturnChannelError::NO_ERROR;

            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                error = ::parseRC(this, cmdID, m_cmdRecv.data(), m_cmdRecv.size());
            }

            if (error || code)
                debug_printf("[RC] cmd 0x%x, error: %d, ret code: %d\n", cmdID, error, code);

            if (error != ReturnChannelError::NO_ERROR)
                return;

            resetWanted(cmdID, code);
        }
#if 1
        else
            pkt.print();
#endif
    }

    bool TxDevice::setWanted(uint16_t cmd)
    {
        static const unsigned TIMER_MS = 4000;

        uint16_t outCmd = RcCommand::in2outID(cmd);
        uint16_t expected = 0;
        if (m_wantedCmd.compare_exchange_strong(expected, outCmd))
        {
            m_timer.restart();
            return true;
        }

        // FIXME: not thread safe!!!
        if (m_timer.elapsedMS() > TIMER_MS)
            m_wantedCmd.store(0);

        return false;
    }

    void TxDevice::resetWanted(uint16_t cmd, uint8_t code)
    {
        if (cmd)
        {
            if (m_wantedCmd.compare_exchange_strong(cmd, 0))
            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                m_responses[cmd] = code;
            }
        }
        else
            m_wantedCmd.store(0);
    }

    bool TxDevice::responseCode(uint16_t cmd, uint8_t& code)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        uint16_t outCmd = RcCommand::in2outID(cmd);

        auto it = m_responses.find(outCmd);
        if (it != m_responses.end())
        {
            code = it->second;
            return true;
        }

        return false;
    }

    void TxDevice::clearResponse(uint16_t cmd)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        uint16_t outCmd = RcCommand::in2outID(cmd);
        m_responses.erase(outCmd);
    }

    bool TxDevice::hasResponses(const uint16_t * cmdInputIDs, unsigned size) const
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        for (unsigned i = 0; i < size; ++i)
        {
            uint16_t outCmd = RcCommand::in2outID(cmdInputIDs[i]);
            auto it = m_responses.find(outCmd);
            if (it == m_responses.end())
                return false;
            else if (it->second != RcCommand::CODE_SUCCESS && it->second != RcCommand::CODE_UNSUPPORTED)
                return false;
        }

        return true;
    }

    bool TxDevice::supported(uint16_t cmdID) const
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        uint16_t outCmd = RcCommand::in2outID(cmdID);
        auto it = m_responses.find(outCmd);
        if (it != m_responses.end())
            return (it->second == RcCommand::CODE_SUCCESS);

        return false;
    }

    uint16_t TxDevice::cmd2update() const
    {
        static const unsigned num = 9;
        static const uint16_t cmdIDs[num] = {
            CMD_GetTransmissionParameterCapabilitiesInput,
            CMD_GetTransmissionParametersInput,
            CMD_GetDeviceInformationInput,
            CMD_GetProfilesInput,
            CMD_GetVideoSourcesInput,
            CMD_GetVideoSourceConfigurationsInput,
            CMD_GetVideoEncoderConfigurationsInput,
            //
            CMD_GetSystemDateAndTimeInput,
            //CMD_GetVideoOSDConfigurationInput,
            CMD_GetOSDInformationInput
        };

        for (size_t i = 0; i < num; ++i)
        {
            if (! hasResponse(cmdIDs[i]))
                return cmdIDs[i];
        }

        m_ready.store(true);
        return 0;
    }


    bool TxDevice::prepareVideoEncoderParams(unsigned streamNo)
    {
        static const uint16_t cmdIDs[] = {
            CMD_GetVideoEncoderConfigurationsInput
        };

        if (! hasResponse(cmdIDs[0]))
            return false;

        //

        if (videoEncConfig.configListSize < streamNo)
            return false;

        VideoEncConfigParam * enc = &videoEncConfig.configList[streamNo];

        videoEncConfigSetParam.name.copy(&enc->name);
        videoEncConfigSetParam.token.copy(&enc->token);

        videoEncConfigSetParam.useCount           = enc->useCount;
        videoEncConfigSetParam.encoding           = enc->encoding;
        videoEncConfigSetParam.width              = enc->width;
        videoEncConfigSetParam.height             = enc->height;
        videoEncConfigSetParam.quality            = enc->quality;
        videoEncConfigSetParam.frameRateLimit     = enc->frameRateLimit;
        videoEncConfigSetParam.encodingInterval   = enc->encodingInterval;
        videoEncConfigSetParam.bitrateLimit       = enc->bitrateLimit;
        videoEncConfigSetParam.rateControlType    = enc->rateControlType;
        videoEncConfigSetParam.govLength          = enc->govLength;
        videoEncConfigSetParam.profile            = enc->profile;
        videoEncConfigSetParam.extensionFlag      = enc->extensionFlag;
        videoEncConfigSetParam.targetBitrateLimit = enc->targetBitrateLimit;
        videoEncConfigSetParam.aspectRatio        = enc->aspectRatio;
        videoEncConfigSetParam.forcePersistence   = enc->forcePersistence;

        return true;
    }

    bool TxDevice::prepareTransmissionParams()
    {
        static const uint16_t cmdIDs[] = {
            CMD_GetTransmissionParameterCapabilitiesInput,
            CMD_GetTransmissionParametersInput
        };

        if (! hasResponses(cmdIDs, 2))
            return false;

        return true;
    }

    bool TxDevice::prepareDateTime()
    {
        static const uint16_t cmdIDs[] = {
            CMD_GetSystemDateAndTimeInput
        };

        if (! hasResponse(cmdIDs[0]))
            return false;

        time_t now = time(nullptr);
        struct tm utc;
        gmtime_r(&now, &utc);

        systemTime.UTCSecond = utc.tm_sec;
        systemTime.UTCMinute = utc.tm_min;
        systemTime.UTCHour = utc.tm_hour;
        systemTime.UTCDay = utc.tm_mday;
        systemTime.UTCMonth = utc.tm_mon + 1;
        systemTime.UTCYear = utc.tm_year + 1900;
        systemTime.daylightSavings = (utc.tm_isdst > 0);

        tzset();

        //systemTime.countryRegionID = 0;
        int hTimezone = timezone / 3600;
        systemTime.timeZone = hTimezone & 0x7f;
        if (hTimezone < 0)
            systemTime.timeZone |= 0x80;

        static const unsigned CC_SIZE = 3;
        unsigned cc = (systemTime.daylightSavings && tzname[1] && tzname[1][0]) ? 1 : 0;
        for (unsigned i = 0; i < CC_SIZE; ++i)
            systemTime.countryCode[i] = tzname[cc][i];

        systemTime.extensionFlag = 0;
        //systemTime.UTCMillisecond = 0;
        //systemTime.timeAdjustmentMode = 0;

        return true;
    }

    bool TxDevice::prepareOSD(unsigned videoSource)
    {
        static const uint16_t cmdIDs[] = {
            CMD_GetVideoOSDConfigurationInput,
            CMD_GetOSDInformationInput
        };

        // *.*Enable       0/1
        // *.*Position     0-5

        if (supported(cmdIDs[0]))
        {
            if (videoSrcConfig.configListSize > videoSource)
                videoOSDConfig.videoSrcToken.copy(&videoSrcConfig.configList[videoSource].token);

            videoOSDConfig.dateEnable = 1;
            videoOSDConfig.datePosition = 0;
            videoOSDConfig.dateFormat = 0;

            videoOSDConfig.timeEnable = 1;
            videoOSDConfig.timePosition = 0;
            videoOSDConfig.timeFormat = 1;

            videoOSDConfig.logoEnable = 1;
            videoOSDConfig.logoPosition = 0;
            videoOSDConfig.logoOption = 0;

            videoOSDConfig.detailInfoEnable = 1;
            videoOSDConfig.detailInfoPosition = 0;
            videoOSDConfig.detailInfoOption = 0;

            videoOSDConfig.textEnable = 1;
            videoOSDConfig.textPosition = 0;
            videoOSDConfig.text.set((const unsigned char *)"text", 4);

            return true;
        }

        if (supported(cmdIDs[1]))
        {
            osdInfo.dateEnable = 1;
            osdInfo.datePosition = 0;
            osdInfo.dateFormat = 0;

            osdInfo.timeEnable = 1;
            osdInfo.timePosition = 0;
            osdInfo.timeFormat = 1;

            osdInfo.logoEnable = 1;
            osdInfo.logoPosition = 0;
            osdInfo.logoOption = 0;

            osdInfo.detailInfoEnable = 1;
            osdInfo.detailInfoPosition = 0;
            osdInfo.detailInfoOption = 0;

            osdInfo.textEnable = 1;
            osdInfo.textPosition = 0;
            osdInfo.text.set((const unsigned char *)"text", 4);

            return true;
        }

        return false;
    }

    //

    bool TxDevice::videoSourceCfg(unsigned encNo, int& width, int& height, float& fps)
    {
        if (encNo >= videoSrcConfig.configListSize)
            return false;

        width = videoSrcConfig.configList[encNo].boundsWidth;
        height = videoSrcConfig.configList[encNo].boundsHeight;
        fps = videoSrcConfig.configList[encNo].maxFrameRate;
        return true;
    }

    bool TxDevice::videoEncoderCfg(unsigned encNo, int& width, int& height, float& fps, int& bitrate)
    {
        if (encNo >= videoEncConfig.configListSize)
            return false;

        width = videoEncConfig.configList[encNo].width;
        height = videoEncConfig.configList[encNo].height;
        fps = videoEncConfig.configList[encNo].frameRateLimit;
        bitrate = videoEncConfig.configList[encNo].bitrateLimit;
        return true;
    }

    //

    RcCommand * TxDevice::mkSetChannel(unsigned channel)
    {
        if (! prepareTransmissionParams())
            return nullptr;

        unsigned freq = freq4chan(channel);

        if (transmissionParameterCapabilities.frequencyMin == 0 ||
            transmissionParameterCapabilities.frequencyMax == 0)
            return nullptr;

        if (freq < transmissionParameterCapabilities.frequencyMin ||
            freq > transmissionParameterCapabilities.frequencyMax)
            return nullptr;

        transmissionParameter.frequency = freq;

        return mkRcCmd(CMD_SetTransmissionParametersInput);
    }

    RcCommand * TxDevice::mkSetEncoderParams(unsigned streamNo, unsigned fps, unsigned bitrateKbps)
    {
        if (! prepareVideoEncoderParams(streamNo))
            return nullptr;

        // TODO: min/max

        videoEncConfigSetParam.frameRateLimit = fps;
        videoEncConfigSetParam.bitrateLimit = bitrateKbps;

        return mkRcCmd(CMD_SetVideoEncoderConfigurationInput);
    }

    //

    RcCommand * TxDevice::mkRcCmd(uint16_t command)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        m_cmdSend.clear();
        unsigned err = ::makeRcCommand(this, command); // --> ret_chan
        if (! err && m_cmdSend.isValid())
            return &m_cmdSend;
        return nullptr;
    }

    uint8_t * TxDevice::rc_getBuffer(unsigned size)  // <-- ret_chan
    {
        m_cmdSend.resize(size);
        return m_cmdSend.data();
    }

    unsigned TxDevice::rc_command(Byte * , unsigned bufferSize) // <-- ret_chan
    {
        // data is already in m_cmd.data()
        m_cmdSend.resize(bufferSize);
        return 0;
    }

    Security& TxDevice::rc_security()
    {
        m_recvSecurity.userName.clear();
        m_recvSecurity.password.clear();

        return m_recvSecurity;
    }

    bool TxDevice::rc_checkSecurity(Security& )
    {
        return true;
    }

    //

#if 0
    void TxDevice::print() const
    {
        printf("TX ID: %04x\n", m_device.txID);
        printf("Frequency in KHz (ex. 666000): %u\n", transmissionParameter.frequency);
        printf("Bandwidth in MHz (ex. 1~8): %d\n", transmissionParameter.bandwidth);
        //
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
    }
#endif
}
