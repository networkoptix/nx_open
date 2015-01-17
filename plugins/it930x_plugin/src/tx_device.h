#ifndef RETURN_CHANNEL_DEVICE_INFO_H
#define RETURN_CHANNEL_DEVICE_INFO_H

#include <memory>

#include "ret_chan/ret_chan_cmd_host.h"
#include "rc_command.h"

namespace ite
{
    typedef enum
    {
        RCERR_NO_ERROR = 0,
        RCERR_NO_DEVICE,
        RCERR_SYNTAX,
        RCERR_CHECKSUM,
        RCERR_SEQUENCE,
        RCERR_WRONG_LENGTH,
        RCERR_RET_CODE,
        RCERR_NOT_PARSED,
        RCERR_WRONG_CMD,
        RCERR_METADATA,
        RCERR_OTHER
    } RCError;

    struct TxManufactureInfo
    {
        std::string companyName;
        std::string modelName;
        std::string firmwareVersion;
        std::string serialNumber;
        std::string hardwareId;
    };

    struct TxVideoEncConfig
    {
        uint16_t width;
        uint16_t height;
        uint16_t bitrateLimit;
        uint8_t  frameRateLimit;
        uint8_t  quality;

        TxVideoEncConfig()
        :   width(0),
            height(0),
            bitrateLimit(0),
            frameRateLimit(0),
            quality(0)
        {}
    };

    ///
    class TxDevice
    {
    public:
        static const unsigned SLEEP_TIME_MS = 200;
        static const unsigned SEND_WAIT_TIME_MS = 1000;
        static const unsigned CHANNELS_NUM = 16;

        typedef enum
        {
            FREQ_X = 150000,
            FREQ_CH00 = 177000,
            FREQ_CH01 = 189000,
            FREQ_CH02 = 201000,
            FREQ_CH03 = 213000,
            FREQ_CH04 = 225000,
            FREQ_CH05 = 237000,
            FREQ_CH06 = 249000,
            FREQ_CH07 = 261000,
            FREQ_CH08 = 273000,
            FREQ_CH09 = 363000,
            FREQ_CH10 = 375000,
            FREQ_CH11 = 387000,
            FREQ_CH12 = 399000,
            FREQ_CH13 = 411000,
            FREQ_CH14 = 423000,
            FREQ_CH15 = 473000
        } Frequency;

        static Frequency chanFrequency(unsigned channel)
        {
            switch (channel)
            {
                case 0: return FREQ_CH00;
                case 1: return FREQ_CH01;
                case 2: return FREQ_CH02;
                case 3: return FREQ_CH03;
                case 4: return FREQ_CH04;
                case 5: return FREQ_CH05;
                case 6: return FREQ_CH06;
                case 7: return FREQ_CH07;
                case 8: return FREQ_CH08;
                case 9: return FREQ_CH09;
                case 10: return FREQ_CH10;
                case 11: return FREQ_CH11;
                case 12: return FREQ_CH12;
                case 13: return FREQ_CH13;
                case 14: return FREQ_CH14;
                case 15: return FREQ_CH15;
                default:
                    break;
            }
            return FREQ_X;
        }

        static unsigned freqChannel(unsigned freq)
        {
            switch (freq)
            {
                case FREQ_CH00: return 0;
                case FREQ_CH01: return 1;
                case FREQ_CH02: return 2;
                case FREQ_CH03: return 3;
                case FREQ_CH04: return 4;
                case FREQ_CH05: return 5;
                case FREQ_CH06: return 6;
                case FREQ_CH07: return 7;
                case FREQ_CH08: return 8;
                case FREQ_CH09: return 9;
                case FREQ_CH10: return 10;
                case FREQ_CH11: return 11;
                case FREQ_CH12: return 12;
                case FREQ_CH13: return 13;
                case FREQ_CH14: return 14;
                case FREQ_CH15: return 15;
                default:
                    break;
            }
            return 0xffff;
        }

        TxDevice(unsigned short txID, unsigned freq);
        ~TxDevice();

        RCHostInfo * rcHost() { return &info_; }

        bool setChannel(unsigned channel);

        void resetCmd() { cmd_.clear(); }

        unsigned short sequenceRecv() const { return info_.device.RCCmd_sequence_recv; }
        void setSequenceRecv(unsigned short value) { info_.device.RCCmd_sequence_recv = value; }

        unsigned short txID() const { return info_.device.clientTxDeviceID; }
        //unsigned short rxID() const { return info_.device.hostRxDeviceID; }
        unsigned frequency() const { return info_.transmissionParameter.frequency; }
        void setFrequency(unsigned freq) { info_.transmissionParameter.frequency = freq; }

        void setWaiting(bool wr, unsigned short cmd = 0)
        {
            waitingResponse_ = wr;
            if (wr)
                waitingCmd_ = cmd;
        }

        unsigned short waitingCmd() const { return waitingCmd_; }
        bool isWaiting() const { return waitingResponse_; }
        bool wait(int iWaitTime = SEND_WAIT_TIME_MS);

        RebuiltCmd& cmd() { return cmd_; }
        RCError parseCommand();

        bool isActive() const { return isActive_; }
        void setActive(bool a = true) { isActive_ = a; }

        void print() const;

        //
#if 0
        //void getTxDeviceAddressID()               { sendCmd(CMD_GetTxDeviceAddressIDInput); }
        void getTransmissionParameterCapabilities() { sendCmd(CMD_GetTransmissionParameterCapabilitiesInput); }
        void getTransmissionParameters()            { sendCmd(CMD_GetTransmissionParametersInput); }
        void getHwRegisterValues()                  { sendCmd(CMD_GetHwRegisterValuesInput); }
        void getAdvanceOptions()                    { sendCmd(CMD_GetAdvanceOptionsInput); }
        void getTPSInformation()                    { sendCmd(CMD_GetTPSInformationInput); }
        void getSiPsiTable()                        { sendCmd(CMD_GetSiPsiTableInput); }
        void getNitLocation()                       { sendCmd(CMD_GetNitLocationInput); }
        void getSdtService()                        { sendCmd(CMD_GetSdtServiceInput); }
        void getEITInformation()                    { sendCmd(CMD_GetEITInformationInput); }

        //void setTxDeviceAddressID()               { sendCmd(CMD_SetTxDeviceAddressIDInput); }
        void setCalibrationTable()                  { sendCmd(CMD_SetCalibrationTableInput); }
        void setTransmissionParameters()            { sendCmd(CMD_SetTransmissionParametersInput); }
        void setHwRegisterValues()                  { sendCmd(CMD_SetHwRegisterValuesInput); }
        void setAdvaneOptions()                     { sendCmd(CMD_SetAdvaneOptionsInput); }
        void setTPSInformation()                    { sendCmd(CMD_SetTPSInformationInput); }
        void setSiPsiTable()                        { sendCmd(CMD_SetSiPsiTableInput); }
        void setNitLocation()                       { sendCmd(CMD_SetNitLocationInput); }
        void setSdtService()                        { sendCmd(CMD_SetSdtServiceInput); }
        void setEITInformation()                    { sendCmd(CMD_SetEITInformationInput); }

        //

        void getCapabilities()                      { sendCmd(CMD_GetCapabilitiesInput); }
        void getDeviceInformation()                 { sendCmd(CMD_GetDeviceInformationInput); }
        void getHostname()                          { sendCmd(CMD_GetHostnameInput); }
        void getSystemDateAndTime()                 { sendCmd(CMD_GetSystemDateAndTimeInput); }
        void getSystemLog()                         { sendCmd(CMD_GetSystemLogInput); }
        void getOSDInformation()                    { sendCmd(CMD_GetOSDInformationInput); }

        void setSystemFactoryDefault()              { sendCmd(CMD_SetSystemFactoryDefaultInput); }
        void setHostname()                          { sendCmd(CMD_SetHostnameInput); }
        void setSystemDateAndTime()                 { sendCmd(CMD_SetSystemDateAndTimeInput); }
        void setOSDInformation()                    { sendCmd(CMD_SetOSDInformationInput); }

        void systemReboot()                         { sendCmd(CMD_SystemRebootInput); }
        //void upgradeSystemFirmware()              { sendCmd(CMD_UpgradeSystemFirmwareInput); }

        //

        void getDigitalInputs()                     { sendCmd(CMD_GetDigitalInputsInput); }
        void getRelayOutputs()                      { sendCmd(CMD_GetRelayOutputsInput); }

        void setRelayOutputState()                  { sendCmd(CMD_SetRelayOutputStateInput); }
        void setRelayOutputSettings()               { sendCmd(CMD_SetRelayOutputSettingsInput); }

        //

        void getImagingSettings()                   { sendCmd(CMD_GetImagingSettingsInput); }
        void imgGetStatus()                         { sendCmd(CMD_IMG_GetStatusInput); }
        void imgGetOptions()                        { sendCmd(CMD_IMG_GetOptionsInput); }
        void getUserDefinedSettings()               { sendCmd(CMD_GetUserDefinedSettingsInput); }
        void setImagingSettings()                   { sendCmd(CMD_SetImagingSettingsInput); }
        void imgMove()                              { sendCmd(CMD_IMG_MoveInput); }
        void imgStop()                              { sendCmd(CMD_IMG_StopInput); }
        void setUserDefinedSettings()               { sendCmd(CMD_SetUserDefinedSettingsInput); }

        //

        void getProfiles()                          { sendCmd(CMD_GetProfilesInput); }
        void getVideoSources()                      { sendCmd(CMD_GetVideoSourcesInput); }
        void getVideoSourceConfigurations()         { sendCmd(CMD_GetVideoSourceConfigurationsInput); }
        void getNumberOfVideoEncoderInstances()     { sendCmd(CMD_GetGuaranteedNumberOfVideoEncoderInstancesInput); }
        void getVideoEncoderConfigurations()        { sendCmd(CMD_GetVideoEncoderConfigurationsInput); }
        void getAudioSources()                      { sendCmd(CMD_GetAudioSourcesInput); }
        void getAudioSourceConfigurations()         { sendCmd(CMD_GetAudioSourceConfigurationsInput); }
        void getAudioEncoderConfigurations()        { sendCmd(CMD_GetAudioEncoderConfigurationsInput); }
        void getVideoSourceConfigurationOptions()   { sendCmd(CMD_GetVideoSourceConfigurationOptionsInput); }
        void getVideoEncoderConfigurationOptions()  { sendCmd(CMD_GetVideoEncoderConfigurationOptionsInput); }
        void getAudioSourceConfigurationOptions()   { sendCmd(CMD_GetAudioSourceConfigurationOptionsInput); }
        void getAudioEncoderConfigurationOptions()  { sendCmd(CMD_GetAudioEncoderConfigurationOptionsInput); }
        void getVideoOSDConfiguration()             { sendCmd(CMD_GetVideoOSDConfigurationInput); }
        void getVideoPrivateArea()                  { sendCmd(CMD_GetVideoPrivateAreaInput); }

        void setSynchronizationPoint()              { sendCmd(CMD_SetSynchronizationPointInput); }
        void setVideoSourceConfiguration()          { sendCmd(CMD_SetVideoSourceConfigurationInput); }
        void setVideoEncoderConfiguration()         { sendCmd(CMD_SetVideoEncoderConfigurationInput); }
        void setAudioSourceConfiguration()          { sendCmd(CMD_SetAudioSourceConfigurationInput); }
        void setAudioEncoderConfiguration()         { sendCmd(CMD_SetAudioEncoderConfigurationInput); }
        void setVideoOSDConfiguration()             { sendCmd(CMD_SetVideoOSDConfigurationInput); }
        void setVideoPrivateArea()                  { sendCmd(CMD_SetVideoPrivateAreaInput); }
        void setVideoSourceControl()                { sendCmd(CMD_SetVideoSourceControlInput); }

        //

        void getConfigurations()                    { sendCmd(CMD_GetConfigurationsInput); }
        void ptzGetStatus()                         { sendCmd(CMD_PTZ_GetStatusInput); }
        void getPresets()                           { sendCmd(CMD_GetPresetsInput); }
        void gotoPreset()                           { sendCmd(CMD_GotoPresetInput); }
        void removePreset()                         { sendCmd(CMD_RemovePresetInput); }
        void setPreset()                            { sendCmd(CMD_SetPresetInput); }
        void absoluteMove()                         { sendCmd(CMD_AbsoluteMoveInput); }
        void relativeMove()                         { sendCmd(CMD_RelativeMoveInput); }
        void continuousMove()                       { sendCmd(CMD_ContinuousMoveInput); }
        void setHomePosition()                      { sendCmd(CMD_SetHomePositionInput); }
        void gotoHomePosition()                     { sendCmd(CMD_GotoHomePositionInput); }
        void ptzStop()                              { sendCmd(CMD_PTZ_StopInput); }

        //

        void getSupportedRules()                    { sendCmd(CMD_GetSupportedRulesInput); }
        void getRules()                             { sendCmd(CMD_GetRulesInput); }
        void createRule()                           { sendCmd(CMD_CreateRuleInput); }
        void modifyRule()                           { sendCmd(CMD_ModifyRuleInput); }
        void deleteRule()                           { sendCmd(CMD_DeleteRuleInput); }
#endif

        // info

        TxManufactureInfo txDeviceInfo() const
        {
            TxManufactureInfo mInfo;
            mInfo.companyName = rcStr2str(info_.manufactureInfo.manufactureName);
            mInfo.modelName = rcStr2str(info_.manufactureInfo.modelName);
            mInfo.firmwareVersion = rcStr2str(info_.manufactureInfo.firmwareVersion);
            mInfo.serialNumber = rcStr2str(info_.manufactureInfo.serialNumber);
            mInfo.hardwareId = rcStr2str(info_.manufactureInfo.hardwareId);
            return mInfo;
        }

        // encoder

        uint8_t encodersCount() const { return info_.videoEncConfig.configListSize; }

        TxVideoEncConfig txVideoEncConfig(uint8_t encoderNo)
        {
            if (encoderNo >= encodersCount())
                return TxVideoEncConfig();

            TxVideoEncConfig conf;
            conf.width = info_.videoEncConfig.configList[encoderNo].width;
            conf.height = info_.videoEncConfig.configList[encoderNo].height;
            conf.bitrateLimit = info_.videoEncConfig.configList[encoderNo].bitrateLimit;
            conf.frameRateLimit = info_.videoEncConfig.configList[encoderNo].frameRateLimit;
            conf.quality = info_.videoEncConfig.configList[encoderNo].quality;
            return conf;
        }

        //bool setVideoEncConfig(unsigned streamNo, const TxVideoEncConfig& conf);

    private:
        RCHostInfo info_;
        RebuiltCmd cmd_;
        unsigned short waitingCmd_;
        bool waitingResponse_;
        bool isActive_;
#if 0
        void sendCmd(unsigned short command)
        {
            setWaiting(true, input2output(command));

            info_.cmdSendConfig.bIsCmdBroadcast = False;
            unsigned error = Cmd_Send(&info_, command);
            if (error)
                throw "Can't send cmd";
        }
#endif
        static std::string rcStr2str(const RCString& s) { return std::string((const char *)s.stringData, s.stringLength); }

        TxDevice(const TxDevice& );
        TxDevice& operator = (const TxDevice& );
    };

    typedef std::shared_ptr<TxDevice> TxDevicePtr;
}

#endif
