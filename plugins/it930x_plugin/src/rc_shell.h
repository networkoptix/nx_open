#ifndef RETURN_CHANNEL_SHELL_H
#define RETURN_CHANNEL_SHELL_H

#include <stdint.h>
#include <mutex>
#include <vector>
#include <memory>

#include "rc_cmd.h"

#include "ret_chan/ret_chan_cmd.h"
#include "ret_chan/ret_chan_user.h"
#include "ret_chan/ret_chan_user_host.h"
#include "ret_chan/ret_chan_cmd_host.h"

namespace ite
{
    ///
    class DeviceInfo
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

        DeviceInfo(unsigned short rxID = 0, unsigned short txID = 0, bool active = false);
        ~DeviceInfo();

        RCHostInfo * xPtr() { return &info_; }

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

        bool setChannel(unsigned channel, bool sendRequest = true);

        // <encoder>

        bool setEncoderCfg(unsigned streamNo, unsigned bitrate, unsigned fps);

        uint8_t encodersCount() const { return info_.videoEncConfig.configListSize; }

        bool resolution(uint8_t encoderNo, uint16_t& width, uint16_t& height) const
        {
            if (encoderNo >= encodersCount())
                return false;

            width = info_.videoEncConfig.configList[encoderNo].width;
            height = info_.videoEncConfig.configList[encoderNo].height;
            return true;
        }

        bool framerate(uint8_t encoderNo, uint8_t& fps) const
        {
            if (encoderNo >= encodersCount())
                return false;

            fps = info_.videoEncConfig.configList[encoderNo].frameRateLimit;
            return true;
        }

        bool bitrate(uint8_t encoderNo, uint16_t& bitrate) const
        {
            if (encoderNo >= encodersCount())
                return false;

            bitrate = info_.videoEncConfig.configList[encoderNo].bitrateLimit;
            return true;
        }

        bool quality(uint8_t encoderNo, uint16_t& q) const
        {
            if (encoderNo >= encodersCount())
                return false;

            q = info_.videoEncConfig.configList[encoderNo].quality;
            return true;
        }

        // </encoder>

        static unsigned short input2output(unsigned short command)
        {
            if (command >= CMD_GetMetadataSettingsInput)
                return command | 0xF000;
            return command | 0x8000;
        }

        void resetCmd() { cmd_.clear(); }

        unsigned short txID() const { return info_.device.clientTxDeviceID; }
        unsigned short rxID() const { return info_.device.hostRxDeviceID; }
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

        bool isActive() const { return isActive_; }
        void setActive(bool a = true) { isActive_ = a; }

        void print() const;

        //

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

    private:
        RCHostInfo info_;
        RebuiltCmd cmd_;
        unsigned short waitingCmd_;
        bool waitingResponse_;
        bool isActive_;

        void sendCmd(unsigned short command)
        {
            setWaiting(true, input2output(command));

            info_.cmdSendConfig.bIsCmdBroadcast = False;
            unsigned error = Cmd_Send(&info_, command);
            if (error)
                throw "Can't send cmd";
        }

        DeviceInfo(const DeviceInfo& );
        DeviceInfo& operator = (const DeviceInfo& );
    };

    typedef std::shared_ptr<DeviceInfo> DeviceInfoPtr;

    ///
    struct DebugInfo
    {
        unsigned totalGetBufferCount;
        unsigned totalPKTCount;
        unsigned leadingTagErrorCount;
        unsigned endTagErrorCount;
        unsigned lengthErrorCount;
        unsigned checkSumErrorCount;
        unsigned sequenceErrorCount;

        DebugInfo()
        {
            reset();
        }

        void print() const;
        void reset();
    };

    ///
    struct IDsLink
    {
        unsigned short rxID;
        unsigned short txID;
        unsigned frequency;
        bool isActive;
    };

    ///
    class RCShell
    {
    public:
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
        } Error;

        static const char * getErrorStr(Error e);

        RCShell();
        ~RCShell();

        void startRcvThread();
        void stopRcvThread();
        bool isRun() const { return bIsRun_; }

        bool sendGetIDs(int iWaitTime = DeviceInfo::SEND_WAIT_TIME_MS * 2);

        bool addDevice(unsigned short rxID, unsigned short txID, bool rcActive);
        Error processCommand(Command& cmd);
        Error lastError() const { return lastError_; }

        DebugInfo& debugInfo() { return debugInfo_; }

        void updateDevsParams();
        void getDevIDs(std::vector<IDsLink>& outLinks);
        DeviceInfoPtr device(const IDsLink& idl) const;

        bool setChannel(unsigned short txID, unsigned channel);
        void setRxFrequency(unsigned short rxID, unsigned frequency);

    private:
        mutable std::mutex mutex_;
        std::vector<DeviceInfoPtr> devs_;
        std::vector<unsigned> frequencies_;
        pthread_t rcvThread_;
        Error lastError_;
        bool bIsRun_;

        DebugInfo debugInfo_;
        //void waitBlockBcast(Word command, int waitTime);
    };
}

#endif
