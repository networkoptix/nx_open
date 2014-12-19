#ifndef RETURN_CHANNEL_SHELL_H
#define RETURN_CHANNEL_SHELL_H

#include <cstdint>
#include <mutex>
#include <vector>
#include <deque>
#include <map>

#include "rc_com.h"
#include "rc_device_info.h"

namespace ite
{
    ///
    struct DebugInfo
    {
        unsigned totalGetBufferCount;
        unsigned totalPKTCount;
        unsigned tagErrorCount;
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
        typedef std::shared_ptr<RCCommand> RCCommandPtr;

        RCShell();
        ~RCShell();

        void startRcvThread();
        void stopRcvThread();
        bool isRun() const { return bIsRun_; }

        bool sendGetIDs(int iWaitTime = DeviceInfo::SEND_WAIT_TIME_MS * 2);

        DeviceInfoPtr addDevice(unsigned short rxID, unsigned short txID, unsigned frequency, bool rcActive);
        void removeDevice(unsigned short txID);

        DebugInfo& debugInfo() { return debugInfo_; }

        void getDevIDs(std::vector<IDsLink>& outLinks);
        DeviceInfoPtr device(unsigned short rxID, unsigned short txID) const;

        void updateTxParams(DeviceInfoPtr dev);

        bool setChannel(unsigned short rxID, unsigned short txID, unsigned channel);
        void setRxFrequency(unsigned short rxID, unsigned frequency);
        unsigned lastTxFrequency(unsigned short txID) const;

        void processCommand(DeviceInfoPtr dev, unsigned short cmdID);

        void push_back(RCCommandPtr cmd);
        RCCommandPtr pop_front();

    private:
        mutable std::mutex mutex_;
        mutable std::mutex mutexUART_;
        std::deque<RCCommandPtr> cmds_;
        std::vector<DeviceInfoPtr> devs_;
        std::vector<unsigned> frequencies_;                     // RxID as index
        std::map<unsigned short, unsigned short> lastRx4Tx_;    // {TxID, RxID}
        pthread_t rcvThread_;
        pthread_t parseThread_;
        bool bIsRun_;
        ComPort comPort_;

        DebugInfo debugInfo_;
    };
}

#endif
