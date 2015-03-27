#ifndef ITE_RX_DEVICE_H
#define ITE_RX_DEVICE_H

#include <string>
#include <memory>
#include <mutex>

#include "it930x.h"
#include "dev_reader.h"

namespace ite
{
    unsigned str2num(std::string& s);
    std::string num2str(unsigned id);

    class TxDevice;
    typedef std::shared_ptr<TxDevice> TxDevicePtr;

    ///
    struct DevLink
    {
        uint16_t rxID;
        uint16_t txID;
        unsigned channel;

        DevLink()
        :   rxID(0), txID(0), channel(0)
        {}
    };

    ///
    class RxDevice
    {
    public:
        constexpr static const char * DEVICE_PATTERN = "usb-it930x";

        enum
        {
            NOT_A_CHANNEL = 16 // TxDevice::CHANNELS_NUM
        };

        static constexpr unsigned MIN_STRENGTH() { return 60; }
        static constexpr unsigned MIN_QUALITY() { return 60; }

        static uint16_t stream2pid(unsigned stream)
        {
            switch (stream)
            {
                case 0: return Pacidal::PID_VIDEO_FHD;
                case 1: return Pacidal::PID_VIDEO_SD;
                default:
                    return Pacidal::PID_VIDEO_FHD;
            }
        }
#if 0
        static unsigned pid2stream(uint16_t pid)
        {
            switch (pid)
            {
                case Pacidal::PID_VIDEO_FHD: return 0;
                case Pacidal::PID_VIDEO_SD: return 1;
                default:
                    return 0;
            }
        }
#endif
        static constexpr unsigned streamsCountPassive() { return 2; }
        unsigned streamsCount();

        static void resolutionPassive(unsigned stream, int& width, int& height, float& fps);
        void resolution(unsigned stream, int& width, int& height, float& fps);

        //

        RxDevice(unsigned id);

        unsigned short rxID() const { return m_rxID; }
        unsigned short txID() const
        {
            if (m_channel <= NOT_A_CHANNEL)
                return m_txs[m_channel].present;
            return 0;
        }

        DevReader * reader() { return m_devReader.get(); }
        bool isReading() const { return m_devReader->hasThread(); }

        bool subscribe(unsigned stream) { return m_devReader->subscribe(stream2pid(stream)); }
        void unsubscribe(unsigned stream) { m_devReader->unsubscribe(stream2pid(stream)); }

        bool lockCamera(TxDevicePtr txDev);
        bool tryLockC(unsigned freq, bool prio = true);
        bool isLocked() const;
        void unlockC();

        void startSearchTx(unsigned channel);
        void startSearchLost(unsigned lostNum);
        void stopSearchTx(DevLink& outDevLink);

        void processRcQueue();
        void updateTxParams();

        const IteDriverInfo rxDriverInfo() const { return m_rxInfo; }
        TxDevicePtr txDevice() const { return m_txDev; }

        unsigned channel() { return m_channel; }
        uint8_t strength() const { return m_signalStrength; }
        uint8_t quality() const { return m_signalQuality; }
        bool present() const { return m_signalPresent; }
        bool good() const { return present() && strength() > MIN_STRENGTH() /*&& quality() > MIN_QUALITY()*/; }

        static unsigned dev2id(const std::string& devName);
        static unsigned str2id(const std::string& devName);
        static unsigned str2id(const char * devName)
        {
            std::string name(devName);
            return str2id(name);
        }

        static std::string id2str(unsigned id);

        // Encoder stuff

        // TODO: getResolutions(), setResolution(), getMaxBitrate()
        bool setEncoderParams(unsigned streamNo, unsigned fps, unsigned bitrateKbps);

        // TX stuff

        void setTx(unsigned channel, uint16_t txID = 0)
        {
#if 1
            printf("Rx: %d; Tx: %x (%d); channel: %d\n", rxID(), txID, txID, channel);
#endif
            if (channel >= m_txs.size())
                return;

            if (txID)
                m_txs[channel].set(txID);
            else
                m_txs[channel].lose();
        }

        uint16_t getTx(unsigned channel, bool lost = false) const
        {
            if (channel >= m_txs.size())
                return 0;

            if (lost)
                return m_txs[channel].lost;
            return m_txs[channel].present;
        }

        void checkTx(unsigned channel, uint16_t txID)
        {
#if 1
            printf("restore Rx: %d; Tx: %x (%d); channel: %d\n", rxID(), txID, txID, channel);
#endif
            if (channel >= m_txs.size())
                return;

            m_txs[channel].check(txID);
        }

        unsigned chan4Tx(uint16_t txID) const
        {
            for (size_t i = 0; i < m_txs.size(); ++i)
                if (m_txs[i].present == txID)
                    return i;
            return NOT_A_CHANNEL;
        }

        void setChannel(unsigned channel)
        {
            if (channel >= m_txs.size())
                return;

            if (m_txs[channel].present)
            {
                m_channel = channel;

                for (size_t i = 0; i < m_txs.size(); ++i)
                    m_txs[i].lose();
            }
        }

        bool changeChannel(unsigned channel);

    private:
        ///
        struct TxPresence
        {
            uint16_t present;
            uint16_t lost;

            TxPresence()
            :   present(0), lost(0), m_lock(false)
            {}

            void set(uint16_t txID)
            {
                bool expected = false;
                while (! m_lock.compare_exchange_weak(expected, true))
                    ;

                present = txID;
                lost = 0;

                m_lock.store(false);
            }

            void lose()
            {
                bool expected = false;
                while (! m_lock.compare_exchange_weak(expected, true))
                    ;

                if (present)
                {
                    lost = present;
                    present = 0;
                }

                m_lock.store(false);
            }

            void check(uint16_t txID)
            {
                bool expected = false;
                while (! m_lock.compare_exchange_weak(expected, true))
                    ;

                if (present == 0)
                    lost = txID;

                m_lock.store(false);
            }

        private:
            std::atomic_bool m_lock;
        };

        ///
        struct Sync
        {
            mutable std::mutex mutex;
            std::condition_variable cond;
            std::atomic_bool usedByCamera;
            bool waiting;
            std::atomic_bool searching;

            Sync()
            :   usedByCamera(false),
                waiting(false),
                searching(false)
            {}
        };

        mutable std::mutex m_mutex;
        std::unique_ptr<It930x> m_device;
        std::unique_ptr<DevReader> m_devReader;
        TxDevicePtr m_txDev;                    // locked Tx device (as camera)
        const uint16_t m_rxID;
        bool m_passive;
        unsigned m_channel;
        Sync m_sync;
        std::vector<TxPresence> m_txs;

        // info from DTV receiver
        uint8_t m_signalQuality;
        uint8_t m_signalStrength;
        bool m_signalPresent;
        IteDriverInfo m_rxInfo;

        // locked Tx device

        bool isLocked_u() const { return m_device.get() && m_device->hasStream(); }
        bool wantedByCamera() const;

        bool open();
        bool stats();

        bool sendRC(RcCommand * cmd);
    };

    typedef std::shared_ptr<RxDevice> RxDevicePtr;
}

#endif
