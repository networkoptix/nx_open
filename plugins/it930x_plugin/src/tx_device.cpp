#include <sys/time.h>

#include "ret_chan/ret_chan_user_host.h"

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

#if 0
        CMD_GetTxDeviceAddressIDInput
        CMD_GetTransmissionParameterCapabilitiesInput
        CMD_GetTransmissionParametersInput
        CMD_GetHwRegisterValuesInput
        CMD_GetAdvanceOptionsInput
        CMD_GetTPSInformationInput
        CMD_GetSiPsiTableInput
        CMD_GetNitLocationInput
        CMD_GetSdtServiceInput
        CMD_GetEITInformationInput

        //CMD_SetTxDeviceAddressIDInput
        CMD_SetCalibrationTableInput
        CMD_SetTransmissionParametersInput
        CMD_SetHwRegisterValuesInput
        CMD_SetAdvaneOptionsInput
        CMD_SetTPSInformationInput
        CMD_SetSiPsiTableInput
        CMD_SetNitLocationInput
        CMD_SetSdtServiceInput
        CMD_SetEITInformationInput

        //

        CMD_GetCapabilitiesInput
        CMD_GetDeviceInformationInput
        CMD_GetHostnameInput
        CMD_GetSystemDateAndTimeInput
        CMD_GetSystemLogInput
        CMD_GetOSDInformationInput

        CMD_SetSystemFactoryDefaultInput
        CMD_SetHostnameInput
        CMD_SetSystemDateAndTimeInput
        CMD_SetOSDInformationInput

        CMD_SystemRebootInput
        //CMD_UpgradeSystemFirmwareInput

        //

        CMD_GetDigitalInputsInput
        CMD_GetRelayOutputsInput

        CMD_SetRelayOutputStateInput
        CMD_SetRelayOutputSettingsInput

        //

        CMD_GetImagingSettingsInput
        CMD_IMG_GetStatusInput
        CMD_IMG_GetOptionsInput
        CMD_GetUserDefinedSettingsInput
        CMD_SetImagingSettingsInput
        CMD_IMG_MoveInput
        CMD_IMG_StopInput
        CMD_SetUserDefinedSettingsInput

        //

        CMD_GetProfilesInput
        CMD_GetVideoSourcesInput
        CMD_GetVideoSourceConfigurationsInput
        CMD_GetGuaranteedNumberOfVideoEncoderInstancesInput
        CMD_GetVideoEncoderConfigurationsInput
        CMD_GetAudioSourcesInput
        CMD_GetAudioSourceConfigurationsInput
        CMD_GetAudioEncoderConfigurationsInput
        CMD_GetVideoSourceConfigurationOptionsInput
        CMD_GetVideoEncoderConfigurationOptionsInput
        CMD_GetAudioSourceConfigurationOptionsInput
        CMD_GetAudioEncoderConfigurationOptionsInput
        CMD_GetVideoOSDConfigurationInput
        CMD_GetVideoPrivateAreaInput

        CMD_SetSynchronizationPointInput
        CMD_SetVideoSourceConfigurationInput
        CMD_SetVideoEncoderConfigurationInput
        CMD_SetAudioSourceConfigurationInput
        CMD_SetAudioEncoderConfigurationInput
        CMD_SetVideoOSDConfigurationInput
        CMD_SetVideoPrivateAreaInput
        CMD_SetVideoSourceControlInput

        //

        CMD_GetConfigurationsInput
        CMD_PTZ_GetStatusInput
        CMD_GetPresetsInput
        CMD_GotoPresetInput
        CMD_RemovePresetInput
        CMD_SetPresetInput
        CMD_AbsoluteMoveInput
        CMD_RelativeMoveInput
        CMD_ContinuousMoveInput
        CMD_SetHomePositionInput
        CMD_GotoHomePositionInput
        CMD_PTZ_StopInput

        //

        CMD_GetSupportedRulesInput
        CMD_GetRulesInput
        CMD_CreateRuleInput
        CMD_ModifyRuleInput
        CMD_DeleteRuleInput
#endif

namespace ite
{
    TxDevice::TxDevice(unsigned short txID, unsigned freq)
    :   m_device(txID),
        m_wantedCmd(0)
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

            if (error != ReturnChannelError::NO_ERROR)
            {
#if 1
                printf("[RC] can't parse command %d, err: %d, code: %d\n", cmdID, error, code);
#endif
                return;
            }

            resetWanted(cmdID);
        }
#if 1
        else
            pkt.print();
#endif
    }

    // TODO: reset wanted by timer (if no response)
    uint16_t TxDevice::getWanted()
    {
        uint16_t cmd = id4update();
        if (cmd)
        {
            uint16_t outCmd = RcCommand::in2outID(cmd);
            uint16_t expected = 0;
            if (m_wantedCmd.compare_exchange_strong(expected, outCmd))
               return cmd;
        }

        return 0;
    }

    void TxDevice::resetWanted(uint16_t cmd)
    {
        if (cmd)
        {
            if (m_wantedCmd.compare_exchange_strong(cmd, 0))
            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                m_responses.insert(cmd);
            }
        }
        else
            m_wantedCmd.store(0);
    }

    bool TxDevice::hasResponses(const uint16_t * cmdInputIDs, unsigned size) const
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        for (unsigned i = 0; i < size; ++i)
        {
            uint16_t outCmd = RcCommand::in2outID(cmdInputIDs[i]);
            if (m_responses.find(outCmd) == m_responses.end())
                return false;
        }

        return true;
    }

    // TODO: updates
    uint16_t TxDevice::id4update()
    {
        static const uint16_t cmdIDs[] = {
            CMD_GetTransmissionParameterCapabilitiesInput,
            CMD_GetTransmissionParametersInput,
            CMD_GetDeviceInformationInput,
            CMD_GetVideoSourcesInput,
            CMD_GetVideoSourceConfigurationsInput,
            CMD_GetVideoEncoderConfigurationsInput,
            //
            CMD_GetProfilesInput
            //CMD_GetVideoEncoderConfigurationOptionsInput
        };

        if (m_wantedCmd)
            return 0;

        for (size_t i = 0; i < sizeof(cmdIDs); ++i)
        {
            if (! hasResponses(&cmdIDs[i], 1))
            {
#if 0
                if (cmdIDs[i] == CMD_GetVideoEncoderConfigurationOptionsInput)
                {
                    MediaProfilesParam * profile = &mediaProfiles.mediaProfilesParam[0];
                    VideoEncConfigParam * enc = &videoEncConfig.configList[0];

                    videoEncConfigOptions.profileToken.copy(&profile->videoEncToken);
                    videoEncConfigOptions.videoEncConfigToken.copy(&enc->token);
                }
#endif
                return cmdIDs[i];
            }
        }

        return 0;
    }


    bool TxDevice::prepareVideoEncoderParams(unsigned streamNo)
    {
        static const uint16_t cmdIDs[] = {
            CMD_GetVideoEncoderConfigurationsInput
        };

        if (! hasResponses(cmdIDs, sizeof(cmdIDs)))
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

    void TxDevice::print() const
    {
        printf("TX ID: %04x\n", m_device.txID);
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

    RcCommand * TxDevice::mkSetChannel(unsigned channel)
    {
        static const uint16_t cmdIDs[] = {
            CMD_GetTransmissionParameterCapabilitiesInput,
            CMD_GetTransmissionParametersInput
        };

        if (! hasResponses(cmdIDs, sizeof(cmdIDs)))
            return nullptr;

        //

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

    RcCommand * TxDevice::mkSetBitrate(unsigned streamNo, unsigned bitrateKbps)
    {
        if (! prepareVideoEncoderParams(streamNo))
            return nullptr;

        // TODO: min/max

        videoEncConfigSetParam.bitrateLimit = bitrateKbps;

        return mkRcCmd(CMD_SetVideoEncoderConfigurationInput);
    }

    RcCommand * TxDevice::mkSetFramerate(unsigned streamNo, unsigned fps)
    {
        if (! prepareVideoEncoderParams(streamNo))
            return nullptr;

        // TODO: min/max

        videoEncConfigSetParam.frameRateLimit = fps;

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

    SecurityValid TxDevice::rc_checkSecurity(Security& )
    {
        return Valid;
    }
}
