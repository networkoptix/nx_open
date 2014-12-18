#include <sys/time.h>

#include "rc_shell.h"

namespace ite
{

void fixReadingPosition(Byte * buf, unsigned bufSize)
{
    for (unsigned pos = 0; pos < bufSize-1; ++pos)
    {
        if (buf[pos] == Command::endingTag() && buf[pos+1] == Command::leadingTag())
        {
            int len;
            User_returnChannelBusRx(pos+1, buf, &len);
            break;
        }
    }
}


static const unsigned RC_SLEEP_TIME_MS = 20000;

void * RC_RecvThread(void * data)
{
    const unsigned BUFFER_SIZE = 188;

    Byte replyBuffer[BUFFER_SIZE];

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

        memset(replyBuffer, 0, BUFFER_SIZE);

        int rxlen;
        User_returnChannelBusRx(Command::maxSize(), replyBuffer, &rxlen);

        if (rxlen <= 0)
        {
            cmdCurrent.clear();
            cmdNext.clear();
            useNext = false;

            usleep(RC_SLEEP_TIME_MS);
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

            RCShell::CommandPtr cmd = std::make_shared<Command>();
            cmd->swap(cmdCurrent);

            pShell->push_back(cmd); // <

            cmdCurrent.clear();
        }
        else
        {
            if (! cmdCurrent.hasTags())
            {
                debugInfo.tagErrorCount++;
                fixReadingPosition(replyBuffer, BUFFER_SIZE);
                continue;
            }

            if (! cmdCurrent.isFull())
                debugInfo.lengthErrorCount++;

            if (! cmdCurrent.checksum())
                debugInfo.checkSumErrorCount++;
        }
    }

    return 0;
}

void * RC_ParseThread(void * data)
{
    RCShell * pShell = (RCShell*)data;

    while (pShell->isRun())
    {
        RCShell::CommandPtr cmd = pShell->pop_front(); // <

        if (! cmd)
        {
            usleep(RC_SLEEP_TIME_MS);
            continue;
        }

        unsigned short cmdID = cmd->commandID();
        unsigned short rxID = cmd->rxID();
        unsigned short txID = cmd->txID();
        unsigned frequency = cmd->frequency();
#if 1 // DEBUG
        printf("RC cmd: %x; TxID: %d; RxID: %d; freq: %d\n", cmdID, txID, rxID, frequency);
#endif
        DeviceInfoPtr dev = pShell->addDevice(rxID, txID, frequency, (CMD_GetTxDeviceAddressIDOutput == cmdID));
        if (dev)
        {
            unsigned short seqNo = cmd->sequenceNum();
            if (seqNo != dev->sequenceRecv() + 1)
                pShell->debugInfo().sequenceErrorCount++;
            dev->setSequenceRecv(seqNo);

            unsigned error = dev->cmd().add(*cmd); // rebuild fragments of command
            if (error == ModulatorError_NO_ERROR)
            {
                if (dev->cmd().isOK())
                {
                    dev->parseCommand();                                    // save data: cmd -> deviceInfo
                    pShell->processCommand(dev, dev->cmd().commandID());    // process data from deviceInfo using command ID
                }
            }
        }
    }

    return 0;
}


// DeviceInfo

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

// RCShell

RCShell::RCShell()
:   bIsRun_(false),
    comPort_("/dev/tnt0", 9600)
{
    ComPort::Instance = &comPort_;

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
    pthread_create(&rcvThread_, NULL, RC_RecvThread, (void*) this);
    pthread_create(&parseThread_, NULL, RC_ParseThread, (void*) this);
}

void RCShell::stopRcvThread()
{
    if (bIsRun_)
    {
        bIsRun_ = false;
        pthread_join(rcvThread_, NULL);
        pthread_join(parseThread_, NULL);
    }
}

void RCShell::setRxFrequency(unsigned short rxID, unsigned freq)
{
    std::lock_guard<std::mutex> lock(mutexUART_); // LOCK

    if (frequencies_.size() <= rxID)
        frequencies_.resize(rxID+1);

    frequencies_[rxID] = freq;
}

void RCShell::push_back(CommandPtr cmd)
{
    std::lock_guard<std::mutex> lock(mutexUART_); // LOCK

    // do not add commands without frequency - interferences
    // could lose some valid commands that recieved after frequencies_[rxID] reset (in another thread)
    unsigned short rxID = cmd->rxID();
    if (rxID < frequencies_.size() && frequencies_[rxID])
    {
        cmd->setFrequency(frequencies_[rxID]);
        cmds_.push_back(cmd);
    }
}

RCShell::CommandPtr RCShell::pop_front()
{
    std::lock_guard<std::mutex> lock(mutexUART_); // LOCK

    CommandPtr cmd;
    if (cmds_.size())
    {
        cmd = cmds_.front();
        cmds_.pop_front();
    }

    return cmd;
}

void RCShell::processCommand(DeviceInfoPtr dev, unsigned short commandID)
{
    switch (commandID)
    {
        case CMD_SetTransmissionParametersOutput:
            removeDevice(dev->txID());
            break;

        case CMD_SetVideoEncoderConfigurationOutput:
            // TODO
            break;

        // TODO:
        // CMD_SetTxDeviceAddressIDOutput
        // CMD_MetadataStreamOutput

        default:
            break;
    }
}

bool RCShell::sendGetIDs(int iWaitTime)
{
    DeviceInfo devInfo;
    RCHostInfo * rcHost = devInfo.rcHost();
    rcHost->cmdSendConfig.bIsCmdBroadcast = True;

    unsigned error = Cmd_Send(rcHost, CMD_GetTxDeviceAddressIDInput);
    if (error == ModulatorError_NO_ERROR)
    {
        if (iWaitTime)
            usleep(iWaitTime * 1000);
        return true;
    }

    return false;
}

void RCShell::updateTxParams(DeviceInfoPtr dev)
{
    if (dev)
    {
        dev->getTransmissionParameterCapabilities();
        dev->getTransmissionParameters();
        dev->getDeviceInformation();
        dev->getVideoSources();
        dev->getVideoSourceConfigurations();
        dev->getVideoEncoderConfigurations();

        dev->wait();
    }
}

void RCShell::getDevIDs(std::vector<IDsLink>& outLinks)
{
    outLinks.clear();
    outLinks.reserve(devs_.size());

    std::lock_guard<std::mutex> lock(mutex_); // LOCK

    for (unsigned i = 0; i < devs_.size(); ++i)
    {
        IDsLink idl;
        idl.rxID = devs_[i]->rxID();
        idl.txID = devs_[i]->txID();
        idl.frequency = devs_[i]->frequency();
        idl.isActive = devs_[i]->isActive();

        outLinks.push_back(idl);
    }
}

DeviceInfoPtr RCShell::addDevice(unsigned short rxID, unsigned short txID, unsigned frequency, bool rcActive)
{
    DeviceInfoPtr dev = device(rxID, txID);

    std::lock_guard<std::mutex> lock(mutex_); // LOCK

    if (! dev)
    {
        dev = std::make_shared<DeviceInfo>(rxID, txID);
        devs_.push_back(dev);
    }

    // new and passive devices
    if (! dev->isActive() && frequency)
        dev->setFrequency(frequency);

    // new, passive + active devices if rcActive set
    if (rcActive || ! dev->isActive())
        lastRx4Tx_[txID] = rxID;

    if (rcActive)
        dev->setActive();

    return dev;
}

/// @note O(N)
DeviceInfoPtr RCShell::device(unsigned short rxID, unsigned short txID) const
{
    std::lock_guard<std::mutex> lock(mutex_); // LOCK

    for (unsigned i = 0; i < devs_.size(); ++i)
    {
        if ((devs_[i]->txID() == txID) && (devs_[i]->rxID() == rxID))
            return devs_[i];
    }

    return DeviceInfoPtr();
}

void RCShell::removeDevice(unsigned short txID)
{
    std::lock_guard<std::mutex> lock(mutex_); // LOCK

    std::vector<DeviceInfoPtr> updatedDevs;
    updatedDevs.reserve(devs_.size());

    for (unsigned i = 0; i < devs_.size(); ++i)
    {
        if (devs_[i]->txID() != txID)
            updatedDevs.push_back(devs_[i]);
    }

    devs_.swap(updatedDevs);
}

bool RCShell::setChannel(unsigned short rxID, unsigned short txID, unsigned channel)
{
    std::lock_guard<std::mutex> lock(mutex_); // LOCK

    for (unsigned i = 0; i < devs_.size(); ++i)
    {
        if (devs_[i]->rxID() == rxID && devs_[i]->txID() == txID)
        {
            devs_[i]->setChannel(channel);
            return devs_[i]->isActive();
        }
    }

    return false;
}

unsigned RCShell::lastTxFrequency(unsigned short txID) const
{
    unsigned rxID;

    {
        std::lock_guard<std::mutex> lock(mutex_); // LOCK

        auto it = lastRx4Tx_.find(txID);
        if (it == lastRx4Tx_.end())
            return 0;

        rxID = it->second;
    }

    DeviceInfoPtr dev = device(rxID, txID);
    if (dev)
        return dev->frequency();

    return 0;
}

//

void DebugInfo::reset()
{
    totalGetBufferCount = 0;
    totalPKTCount = 0;
    tagErrorCount = 0;
    lengthErrorCount = 0;
    checkSumErrorCount = 0;
    sequenceErrorCount = 0;
}

void DebugInfo::print() const
{
    printf("\n=== RC DebugInfo ===\n");
    printf("Total GetBuffer Count   = %u\n", totalGetBufferCount);
    printf("Tags Error Count        = %u\n", tagErrorCount);
    printf("Total PKT Count         = %u\n", totalPKTCount);
    printf("CheckSum Error Count    = %u\n", checkSumErrorCount);
    printf("Sequence Error Count    = %u\n", sequenceErrorCount);
    printf("\n=== RC DebugInfo ===\n");
}

} // ret_chan
