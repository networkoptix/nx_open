#ifndef ITE_RX_DEVICE_H
#define ITE_RX_DEVICE_H

#include <string>
#include <memory>
#include <mutex>

#include "it930x.h"
#include "dev_reader.h"
#include "tx_device.h"

namespace ite
{
    unsigned str2num(std::string& s);
    std::string num2str(unsigned id);

    //!
    class RxDevice
    {
    public:
        constexpr static const char * DEVICE_PATTERN = "usb-it930x";

        enum
        {
            NOT_A_CHANNEL = TxDevice::CHANNELS_NUM
        };

        static constexpr unsigned streamsCount() { return 2; }

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
        static void resolution(unsigned stream, int& width, int& height, float& fps)
        {
            uint16_t pid = stream2pid(stream);

            fps = 30.0f;
            switch (pid)
            {
            case Pacidal::PID_VIDEO_FHD:
                width = 1920;
                height = 1080;
                break;

            case Pacidal::PID_VIDEO_HD:
                width = 1280;
                height = 720;
                break;

            case Pacidal::PID_VIDEO_SD:
                width = 640;
                height = 360;
                break;

            case Pacidal::PID_VIDEO_CIF:
                width = 0;
                height = 0;
                break;
            }
        }

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

        bool lockCamera(uint16_t txID);
        bool tryLockC(unsigned freq);
        bool isLocked() const;
        void unlockC();

        bool startSearchTx(unsigned channel);
        bool stopSearchTx(uint16_t& outTxID);

        const IteDriverInfo rxDriverInfo() const { return m_rxInfo; }
        const TxManufactureInfo txDriverInfo() const { return m_txDev->txDeviceInfo(); }
        const TxVideoEncConfig txVideoEncConfig(uint8_t encNo) const { return m_txDev->txVideoEncConfig(encNo); }

        //unsigned frequency() const { return TxDevice::freq4chan(m_channel); }
        unsigned channel() { return m_channel; }
        uint8_t strength() const { return m_signalStrength; }
        uint8_t quality() const { return m_signalQuality; }
        bool present() const { return m_signalPresent; }
        bool good() const { return present() && (strength() > 0); }

        static unsigned dev2id(const std::string& devName);
        static unsigned str2id(const std::string& devName);
        static unsigned str2id(const char * devName)
        {
            std::string name(devName);
            return str2id(name);
        }

        static std::string id2str(unsigned id);

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

        bool changeChannel(uint16_t txID, unsigned channel);

    private:
        struct TX
        {
            uint16_t present;
            uint16_t lost;

            TX()
            :   present(0), lost(0)
            {}

            void set(uint16_t txID)
            {
                present = txID;
                lost = 0;
            }

            void lose()
            {
                if (present)
                    lost = present;
                present = 0;
            }
        };

        struct Sync
        {
            std::mutex mutex;
            std::condition_variable cond;
            std::atomic_bool usedByCamera;
            bool waiting;

            Sync()
            :   usedByCamera(false),
                waiting(false)
            {}
        };

        mutable std::mutex m_mutex;
        std::unique_ptr<It930x> m_device;
        std::unique_ptr<DevReader> m_devReader;
        const uint16_t m_rxID;
        unsigned m_channel;
        Sync m_sync;
        std::vector<TX> m_txs;

        // info from DTV receiver
        uint8_t m_signalQuality;
        uint8_t m_signalStrength;
        bool m_signalPresent;
        IteDriverInfo m_rxInfo;

        // info from RC
        TxDevicePtr m_txDev;

        bool isLocked_u() const { return m_device.get() && m_device->hasStream(); }

        bool open();
        bool stats();
        void updateTxParams();
    };

    typedef std::shared_ptr<RxDevice> RxDevicePtr;
}

#endif
