#include "rc_shell.h"

namespace ite
{

void fixReadingPosition(Byte * buf, unsigned bufSize)
{
    for (unsigned pos = 0; pos < bufSize-1; ++pos)
    {
        if (buf[pos] == RCCommand::endingTag() && buf[pos+1] == RCCommand::leadingTag())
        {
            int len;
            User_returnChannelBusRx(pos+1, buf, &len);
            break;
        }
    }
}


static const unsigned RC_SLEEP_TIME_US = 20000;

void * RC_RecvThread(void * data)
{
    const unsigned BUFFER_SIZE = 188;

    Byte replyBuffer[BUFFER_SIZE];

    RCShell * pShell = (RCShell*)data;

    bool useNext = false;
    RCCommand cmdCurrent, cmdNext;

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
        User_returnChannelBusRx(RCCommand::maxSize(), replyBuffer, &rxlen);

        if (rxlen <= 0)
        {
            cmdCurrent.clear();
            cmdNext.clear();
            useNext = false;

            usleep(RC_SLEEP_TIME_US);
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

            RCShell::RCCommandPtr cmd = std::make_shared<RCCommand>();
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
        RCShell::RCCommandPtr cmd = pShell->pop_front(); // <

        if (! cmd)
        {
            usleep(RC_SLEEP_TIME_US);
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

void RCShell::push_back(RCCommandPtr cmd)
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

RCShell::RCCommandPtr RCShell::pop_front()
{
    std::lock_guard<std::mutex> lock(mutexUART_); // LOCK

    RCCommandPtr cmd;
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

//#define ENABLE_SEND2RC 1
bool RCShell::sendGetIDs(int iWaitTime)
{
#if ENABLE_SEND2RC
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
#endif
    return false;
}

void RCShell::updateTxParams(DeviceInfoPtr dev)
{
#if ENABLE_SEND2RC
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
#endif
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
