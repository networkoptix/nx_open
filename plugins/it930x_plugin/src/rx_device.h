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
        It930Stream * devStream() { return m_devStream.get(); }

        bool open();
        bool isOpen() const { return m_device.get(); }
        void close()
        {
            m_devStream.reset();
            m_device.reset();
        }

        bool lockCamera(unsigned short txID);
        bool lockF(unsigned freq);
        bool isLocked() const { return m_devStream.get(); }
        void unlockF()
        {
            m_devStream.reset();
            m_frequency = 0;
            m_txID = 0;
        }

        void driverInfo(std::string& driverVersion, std::string& fwVersion, std::string& company, std::string& model) const;

        std::mutex& mutex() { return m_mutex; }

        unsigned frequency() const { return m_frequency; }
        uint8_t strength() const { return m_strength; }
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
        std::unique_ptr<It930Stream> m_devStream;
        unsigned short m_rxID;
        unsigned short m_txID; // locked camera txID or 0
        RCShell * m_rcShell;

        // TODO
        unsigned m_frequency;
        uint8_t m_strength;
        bool m_present;

        bool stats();
    };

    typedef std::shared_ptr<RxDevice> RxDevicePtr;
}

#endif
