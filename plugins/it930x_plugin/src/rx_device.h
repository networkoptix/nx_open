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
        unsigned short txID() const { return m_txID; }

        DevReader * reader() { return m_devReader.get(); }
        bool isReading() const { return m_devReader->hasThread(); }

        bool subscribe(unsigned stream) { return m_devReader->subscribe(stream2pid(stream)); }
        void unsubscribe(unsigned stream) { m_devReader->unsubscribe(stream2pid(stream)); }

        bool lockCamera(unsigned short txID, unsigned frequency);
        bool tryLockF(unsigned freq);
        bool isLocked() const;
        void unlockF();
        bool findTx(unsigned freq, uint16_t& outTxID);

        const IteDriverInfo rxDriverInfo() const { return m_rxInfo; }
        const TxManufactureInfo txDriverInfo() const { return m_txDev->txDeviceInfo(); }
        const TxVideoEncConfig txVideoEncConfig(uint8_t encNo) const { return m_txDev->txVideoEncConfig(encNo); }

        unsigned frequency() const { return m_frequency; }
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

        bool setChannel(unsigned short txID, unsigned chan);

    private:
        mutable std::mutex m_mutex;
        std::unique_ptr<It930x> m_device;
        std::unique_ptr<DevReader> m_devReader;
        unsigned short m_rxID;
        unsigned short m_txID; // captured camera txID or 0
        unsigned m_frequency;

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
