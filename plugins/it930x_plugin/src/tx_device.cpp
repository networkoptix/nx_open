#include <sys/time.h>

#include "ret_chan/ret_chan_user_host.h"
#include "ret_chan/ret_chan_user.h"

#include "tx_device.h"

// ret_chan/ret_chan_cmd_host.cpp
unsigned makeRcCommand(ite::TxDevice * tx, unsigned short command);
unsigned parseRC(ite::TxDevice * tx, unsigned short cmdID, const Byte * Buffer, unsigned bufferLength);

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
    :   m_device(txID)
    {
        User_Host_init(this);
        User_askUserSecurity(&security);

        setFrequency(freq);
    }

    TxDevice::~TxDevice()
    {
        User_Host_uninit(this);
    }

    bool TxDevice::parse(RcPacket& pkt)
    {
        if (! pkt.isOK())
            return false;

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (m_cmdRecv.addPacket(pkt) && m_cmdRecv.isValid())
        {
            uint16_t cmdID = m_cmdRecv.commandID();
            uint8_t code = m_cmdRecv.returnCode();
#if 1
            printf("RC command: %d, code: %d\n", cmdID, code);
#endif
            unsigned error = ::parseRC(this, cmdID, m_cmdRecv.data(), m_cmdRecv.size());
            if (error != ReturnChannelError::NO_ERROR)
                return false;
        }
#if 1
        else
            printf("RC packet (part or error)\n");
#endif
        return true;
    }

    // TODO
    void TxDevice::ids4update(std::vector<uint16_t>& ids)
    {
        // FIXME
        static uint8_t num = 0;
        if (num++ != 0)
            return;

        // TODO

        ids.push_back(CMD_GetTransmissionParameterCapabilitiesInput);
        ids.push_back(CMD_GetTransmissionParametersInput);
        //ids.push_back(CMD_GetDeviceInformationInput);
        //ids.push_back(CMD_GetVideoSourcesInput);
        //ids.push_back(CMD_GetVideoSourceConfigurationsInput);
        //ids.push_back(CMD_GetVideoEncoderConfigurationsInput);
    }

    RcCommand * TxDevice::mkSetChannel(unsigned channel)
    {
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

    RcCommand * TxDevice::mkRcCmd(uint16_t command)
    {
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
}
