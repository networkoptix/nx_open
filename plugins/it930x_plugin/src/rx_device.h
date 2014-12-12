#ifndef ITE_RX_DEVICE_H
#define ITE_RX_DEVICE_H

#include <string>
#include <memory>
#include <mutex>

#include "it930x.h"
#include "rc_shell.h"

namespace ite
{
    unsigned str2num(std::string& s);
    std::string num2str(unsigned id);

    class CameraManager;

    //!
    class RxDevice
    {
    public:
        constexpr static const char * DEVICE_PATTERN = "usb-it930x";

        RxDevice(unsigned id, RCShell * rc);

        unsigned short rxID() const { return m_rxID; }
        unsigned short txID() const { return m_txID; }

        It930x * device() { return m_device.get(); }

        bool lockCamera(unsigned short txID);
        bool tryLockF(unsigned freq);
        bool isLocked() const;
        void unlockF();

        const IteDriverInfo& rxDriverInfo() const { return m_rxInfo; }
        const TxManufactureInfo& txDriverInfo() const { return m_txDriverInfo; }

        unsigned frequency() const { return m_frequency; }
        uint8_t strength() const { return m_strength; }
        uint8_t quality() const { return m_quality; }
        bool present() const { return m_present; }

        static unsigned dev2id(const std::string& devName);
        static unsigned str2id(const std::string& devName);
        static unsigned str2id(const char * devName)
        {
            std::string name(devName);
            return str2id(name);
        }

        static std::string id2str(unsigned id);

        /// @note async
        void updateDevParams() { m_rcShell->updateDevParams(m_rxID); }
        void updateTransmissionParams() { m_rcShell->updateTransmissionParams(m_rxID); }

        bool setChannel(unsigned short txID, unsigned chan);

    private:
        mutable std::mutex m_mutex;
        std::unique_ptr<It930x> m_device;
        unsigned short m_rxID;
        unsigned short m_txID; // captured camera txID or 0
        unsigned m_frequency;
        RCShell * m_rcShell;

        // info from DTV receiver
        uint8_t m_strength;
        uint8_t m_quality;
        bool m_present;
        IteDriverInfo m_rxInfo;

        // info from RC
        DeviceInfoPtr m_txInfo;
        TxManufactureInfo m_txDriverInfo;

        bool isLockedUnsafe() const { return m_device.get() && m_device->hasStream(); }

        bool open();
        bool stats();
    };

    typedef std::shared_ptr<RxDevice> RxDevicePtr;
}

#endif
