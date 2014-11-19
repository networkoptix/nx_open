#include <sys/time.h>

#include "rc_shell.h"

namespace ite
{

#if 0
typedef struct
{
    RCShell * pRCShell;
    DeviceInfo * dinfo;
    int iWaitTime;
    bool isSet;
} RCShell_ThreadParam;


static void RC_Shell_WaitForSet_ThreadParam(RCShell_ThreadParam * pThreadParam)
{
    while (1)
        if (pThreadParam->isSet) // TODO: short sleep
            break;
}
#endif


void * RC_Shell_RecvThread(void * data)
{
    Byte replyBuffer[188];

    RCShell * pShell = (RCShell*)data;

    bool useNext = false;
    Command cmdCurrent, cmdNext;

    while (pShell->isRun())
    {
        if (useNext)
        {
            cmdCurrent.swap(cmdNext);
            cmdNext.clear();
            useNext = false;
        }

        int rxlen;
        User_returnChannelBusRx(Command::maxSize(), replyBuffer, &rxlen);

        if (rxlen <= 0)
        {
            cmdCurrent.clear();
            cmdNext.clear();
            useNext = false;

            usleep(50000);
            continue;
        }

        unsigned unread = cmdCurrent.add(replyBuffer, rxlen);
        if (unread)
        {
            useNext = true;
            cmdNext.clear();
            cmdNext.add(replyBuffer + unread, rxlen - unread);
        }

        DebugInfo& debugInfo = pShell->debugInfo();
        debugInfo.totalGetBufferCount++;

        if (cmdCurrent.isOK())
        {
            debugInfo.totalPKTCount++;

            unsigned short curCommand = cmdCurrent.commandID();

            // Register Rx/TxID
            if (curCommand == CMD_GetTxDeviceAddressIDOutput)
            {
                unsigned short rxDeviceID = cmdCurrent.rxDeviceID();
                unsigned short txDeviceID = cmdCurrent.txDeviceID();

                pShell->addDevice(rxDeviceID, txDeviceID);
            }

            pShell->processCommand(cmdCurrent);
            cmdCurrent.clear();
        }
        else
            debugInfo.leadingTagErrorCount++;
    }

    return 0;
}

#if 0
void * RC_Shell_WaitTimeOut(void * data)
{
    RCShell_ThreadParam * pThreadParam = (RCShell_ThreadParam *) data;

    RCShell_ThreadParam threadInfo;
    threadInfo.pRCShell = pThreadParam->pRCShell;
    threadInfo.dinfo = pThreadParam->dinfo;
    threadInfo.wCommand = pThreadParam->wCommand;
    threadInfo.iWaitTime = pThreadParam->iWaitTime;
    pThreadParam->isSet = true;

    threadInfo.pRCShell->checkTimeOut(*threadInfo.dinfo, threadInfo.iWaitTime);
    return 0;
}
#endif

// DeviceInfo

DeviceInfo::DeviceInfo(unsigned short rxID, unsigned short txID)
:   waitingCmd_(CMD_GetTxDeviceAddressIDOutput),
    waitingResponse_(false),
    isOn_(true)
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
    getTransmissionParameterCapabilities();
    if (! wait())
        return false;

    getTransmissionParameters();
    if (! wait())
        return false;

    unsigned freq = DeviceInfo::chanFrequency(channel);
    if (freq < info_.transmissionParameterCapabilities.frequencyMin ||
        freq > info_.transmissionParameterCapabilities.frequencyMax)
        return false;

    info_.transmissionParameter.frequency = freq;
    setTransmissionParameters();
    if (! wait())
        return false;
    return true;
}

bool DeviceInfo::setEncoderCfg(unsigned streamNo, unsigned bitrate, unsigned fps)
{
    getVideoEncoderConfigurations();
    if (! wait())
        return false;

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

    if (bitrate)
        info_.videoEncConfigSetParam.bitrateLimit = bitrate;
    if (fps)
        info_.videoEncConfigSetParam.frameRateLimit = fps;

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

// RCShell

const char * RCShell::getErrorStr(Error e)
{
    switch (e)
    {
        case RCERR_NO_ERROR:
        case RCERR_METADATA:
            return "";
        case RCERR_NO_DEVICE:
            return "no device";
        case RCERR_SYNTAX:
            return "command syntax error";
        case RCERR_CHECKSUM:
            return "wrong checksum";
        case RCERR_SEQUENCE:
            return "sequence error";
        case RCERR_WRONG_LENGTH:
            return "command wrong length";
        case RCERR_RET_CODE:
            return "comand returns error code";
        case RCERR_WRONG_CMD:
            return "unsupported command";
        case RCERR_NOT_PARSED:
            return "can't parse command";
        default:
            break;
    }

    return "other error";
}

RCShell::RCShell()
:    bIsRun_(false)
{
    User_mutex_init();
}

RCShell::~RCShell()
{
    stopRcvThread();
    User_mutex_uninit();
}

void RCShell::startRcvThread()
{
    debugInfo_.reset();

    bIsRun_ = true;
    pthread_create(&rcvThread_, NULL, RC_Shell_RecvThread, (void*) this);
}

void RCShell::stopRcvThread()
{
    if (bIsRun_)
    {
        bIsRun_ = false;
        pthread_join(rcvThread_, NULL);
    }
}

bool RCShell::addDevice(unsigned short rxID, unsigned short txID)
{
    IDsLink idl;
    idl.rxID = rxID;
    idl.txID = txID;

    DeviceInfoPtr devInfo = device(idl);
    if (devInfo)
    {
        devInfo->setOn();
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_); // LOCK

    bool found = false;
    for (size_t i = 0; i < devs_.size(); ++i)
    {
        if (devs_[i]->rxID() == rxID && devs_[i]->txID() == txID) {
            found = true;
            break;
        }
    }

    if (!found)
        devs_.push_back(std::make_shared<DeviceInfo>(rxID, txID)); // FIXME
    return true;
}

RCShell::Error RCShell::processCommand(Command& cmdPart)
{
    std::lock_guard<std::mutex> lock(mutex_); // LOCK

    if (devs_.empty())
        return lastError_ = RCERR_NO_DEVICE;

    unsigned error = cmdPart.headCheck();
    if (error != ModulatorError_NO_ERROR)
    {
        if (error == ReturnChannelError_CMD_SYNTAX_ERROR)
        {
            if (! cmdPart.hasLeadingTag())
                debugInfo_.leadingTagErrorCount++;
            if (! cmdPart.hasEndingTag())
                debugInfo_.endTagErrorCount++;

            return lastError_ = RCERR_SYNTAX;
        }

        if (error == ReturnChannelError_CMD_CHECKSUM_ERROR)
        {
            debugInfo_.checkSumErrorCount++;
            return lastError_ = RCERR_CHECKSUM;
        }

        return lastError_ = RCERR_OTHER;
    }

    DeviceInfoPtr info;
    for (unsigned i = 0; i < devs_.size(); i++)
    {
        info = devs_[i];
        RCHostInfo * deviceInfo = info->xPtr();

        error = cmdPart.deviceCheck(&deviceInfo->device);
        if (error == ReturnChannelError_CMD_SEQUENCE_ERROR)
        {
            debugInfo_.sequenceErrorCount++;
        }
        else if (error != ModulatorError_NO_ERROR)
        {
            info.reset();
            continue; // enother device
        }

        // Rebuild fragments of command
        error = info->cmd().add(cmdPart);
        if (error == ModulatorError_NO_ERROR)
            break;
        else
            info.reset(); // continue
    }

    if (info && info->cmd().isOK())
    {
        RCHostInfo * deviceInfo = info->xPtr();

        if (info->cmd().size() < 8)
            return lastError_ = RCERR_WRONG_LENGTH;

        if (!info->cmd().isChecksumOK())
            return lastError_ = RCERR_CHECKSUM;

        if (info->cmd().returnCode() != 0)
            return lastError_ = RCERR_RET_CODE;

        unsigned short command = info->cmd().commandID();
        unsigned error = Cmd_HostParser(deviceInfo, command, info->cmd().data(), info->cmd().size());

        if (info->isWaiting() && info->waitingCmd() == command)
            info->setWaiting(false);

        if (error == ModulatorError_NO_ERROR)
        {
            info->cmd().setValid();
            if (command != CMD_MetadataStreamOutput)
                return RCERR_METADATA;
            return RCERR_NO_ERROR;
        }
        else if (error == ReturnChannelError_Reply_WRONG_LENGTH)
            return lastError_ = RCERR_WRONG_LENGTH;
        else if (error == ReturnChannelError_CMD_NOT_SUPPORTED)
            return lastError_ = RCERR_WRONG_CMD;

        return lastError_ = RCERR_NOT_PARSED;
    }

    return lastError_ = RCERR_NO_DEVICE;
}

bool RCShell::sendGetIDs(int iWaitTime)
{
    DeviceInfo devInfo;
    RCHostInfo * deviceInfo = devInfo.xPtr();
    deviceInfo->cmdSendConfig.bIsCmdBroadcast = True;

    unsigned error = Cmd_Send(deviceInfo, CMD_GetTxDeviceAddressIDInput);
    if (error == ModulatorError_NO_ERROR)
    {
        if (iWaitTime)
            usleep(iWaitTime * 1000);
        return true;
    }

    return false;
}

void RCShell::getDevIDsActivity(std::vector<IDsLink>& outLinks)
{
    {
        std::lock_guard<std::mutex> lock(mutex_); // LOCK

        for (unsigned i = 0; i < devs_.size(); ++i)
            devs_[i]->setOff();
    }

    bool ok = sendGetIDs();
    if (!ok)
        return;

    getDevIDs(outLinks);
}

void RCShell::getDevIDs(std::vector<IDsLink>& outLinks)
{
    outLinks.clear();
    outLinks.reserve(devs_.size());

    std::lock_guard<std::mutex> lock(mutex_); // LOCK

    for (unsigned i = 0; i < devs_.size(); ++i)
    {
        if (devs_[i]->isOn())
        {
            devs_[i]->getTransmissionParameters();
            devs_[i]->wait();
        }

        IDsLink idl;
        idl.rxID = devs_[i]->rxID();
        idl.txID = devs_[i]->txID();
        idl.frequency = devs_[i]->frequency();
        idl.isOn = devs_[i]->isOn();

        outLinks.push_back(idl);
    }
}

bool RCShell::getDevIDsChannel(unsigned channel, std::vector<IDsLink>& outLinks)
{
    bool hasOn = false;
    outLinks.clear();
    //outLinks.reserve(devs_.size()); Don't reserve, expect 0.

    std::lock_guard<std::mutex> lock(mutex_); // LOCK

    for (unsigned i = 0; i < devs_.size(); ++i)
    {
        if (devs_[i]->frequency() == DeviceInfo::chanFrequency(channel))
        {
            hasOn |= devs_[i]->isOn();

            IDsLink idl;
            idl.rxID = devs_[i]->rxID();
            idl.txID = devs_[i]->txID();
            idl.frequency = devs_[i]->frequency();
            idl.isOn = devs_[i]->isOn();

            outLinks.push_back(idl);
        }
    }

    return hasOn;
}

/// @note O(N)
DeviceInfoPtr RCShell::device(const IDsLink& idl) const
{
    std::lock_guard<std::mutex> lock(mutex_); // LOCK

    for (unsigned i = 0; i < devs_.size(); ++i)
    {
        if ((devs_[i]->txID() == idl.txID) && (devs_[i]->rxID() == idl.rxID))
            return devs_[i];
    }

    return DeviceInfoPtr();
}

bool RCShell::setChannel(const IDsLink& idl, unsigned channel)
{
    DeviceInfoPtr devInfo = device(idl);
    if (!devInfo)
        return false;

    if (devInfo->frequency() == DeviceInfo::chanFrequency(channel))
        return true;

    std::vector<IDsLink> idls;
    bool hasOn = getDevIDsChannel(channel, idls);
    if (hasOn)
        return false;

    return devInfo->setChannel(channel);
}

#if 0
void RCShell::waitBlockBcast(Word wCommand, int iWaitTime)
{
    std::vector<RCShell_ThreadParam> threadParam( deviceInfo_.size() );
    std::vector<pthread_t> threadHandle( deviceInfo_.size() );

    for (unsigned i = 0; i < deviceInfo_.size(); ++i)
    {
        deviceInfo_[i]->setCmdTimeout(wCommand);

        threadParam[i].pRCShell = this;
        threadParam[i].dinfo = deviceInfo_[i].get();
        threadParam[i].wCommand = wCommand;
        threadParam[i].iWaitTime = iWaitTime;
        threadParam[i].cbFail = cbFail;
        threadParam[i].isSet = false;
    }

    for (unsigned i = 0; i < deviceInfo_.size(); ++i)
        pthread_create(&threadHandle[i], NULL, RC_Shell_WaitTimeOut, (void*) &threadParam[i]);

    for (unsigned i = 0; i < deviceInfo_.size(); ++i)
        RC_Shell_WaitForSet_ThreadParam(&threadParam[i]);

    for (unsigned i = 0; i < deviceInfo_.size(); ++i)
        pthread_join(threadHandle[i], NULL);
}
#endif

void RCShell::printDevices()
{
    printf("================= Device Info List =================\n");

    for (unsigned i = 0; i < devs_.size(); ++i)
    {
        printf("Device No: %d\n", i);
        devs_[i]->print();
        printf("\n");
    }
}

//

void DebugInfo::reset()
{
    totalGetBufferCount = 0;
    totalPKTCount = 0;
    leadingTagErrorCount = 0;
    endTagErrorCount = 0;
    checkSumErrorCount = 0;
    sequenceErrorCount = 0;
}

void DebugInfo::print() const
{
    printf("\n=== User_DebugInfo_log ===\n");
    printf("Total GetBuffer Count   = %u\n", totalGetBufferCount);
    printf("Leading Tag Error Count = %u\n", leadingTagErrorCount);
    printf("Total PKT Count         = %u\n", totalPKTCount);
    printf("End Tag Error Count     = %u\n", endTagErrorCount);
    printf("CheckSum Error Count    = %u\n", checkSumErrorCount);
    printf("Sequence Error Count    = %u\n", sequenceErrorCount);
    printf("\n=== User_DebugInfo_log ===\n");
}

} // ret_chan
