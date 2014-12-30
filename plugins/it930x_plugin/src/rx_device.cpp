#include <sstream>

#include "rx_device.h"
#include "camera_manager.h"

namespace ite
{
    unsigned str2num(std::string& s)
    {
        unsigned value;
        std::stringstream ss;
        ss << s;
        ss >> value;
        return value;
    }

    std::string num2str(unsigned id)
    {
        std::stringstream ss;
        ss << id;
        return ss.str();
    }

    //

    RxDevice::RxDevice(unsigned id, RCShell * rc)
    :   m_rxID(id),
        m_txID(0),
        m_frequency(0),
        m_rcShell(rc),
        m_signalQuality(0),
        m_signalStrength(0),
        m_signalPresent(false)
    {
        open();
    }

    bool RxDevice::open()
    {
        try
        {
            m_device.reset(); // dtor first
            m_device.reset(new It930x(m_rxID));
            m_device->info(m_rxInfo);
            return true;
        }
        catch(const char * msg)
        {
            m_device.reset();
        }

        return false;
    }

    bool RxDevice::setChannel(unsigned short txID, unsigned chan)
    {
        if (isLocked())
            return m_rcShell->setChannel(rxID(), txID, chan);
        return false;
    }

    bool RxDevice::lockCamera(unsigned short txID, unsigned freq)
    {
        static const unsigned DELAY_MS = 20;
        static const unsigned TIMES = (DeviceInfo::SEND_WAIT_TIME_MS * 2) / DELAY_MS;

        // could be locked in discovery thread: waiting
        for (unsigned i = 0; i < TIMES; ++i)
        {
            if (m_txInfo) // locked by another camera
                return false;

            if (tryLockF(freq))
            {
                m_txID = txID;
                updateTxParams();
                return true;
            }

            usleep(DELAY_MS * 1000);
        }

        return false;
    }

    bool RxDevice::tryLockF(unsigned freq)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        // prevent double locking at all (even with the same frequency)
        if (isLockedUnsafe())
            return false;

        if (!m_device && !open())
            return false;

        try
        {
            m_device->lockChannel(freq);
            m_frequency = freq;

            stats();
            return true;
        }
        catch (const char * msg)
        {
            m_device.reset();
        }

        return false;
    }

    void RxDevice::unlockF()
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (!m_device)
            return;

        m_device->closeStream();
        m_txInfo.reset();
        m_frequency = 0;
        m_txID = 0;
    }

    bool RxDevice::isLocked() const
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        return isLockedUnsafe();
    }

    bool RxDevice::stats()
    {
        if (!m_device)
            return false;

        try
        {
            m_signalQuality = 0;
            m_signalStrength = 0;
            m_signalPresent = false;
            bool locked = false;
            m_device->statistic(m_signalQuality, m_signalStrength, m_signalPresent, locked);
            return true;
        }
        catch (const char * msg)
        {
            m_device.reset();
        }

        return false;
    }

    void RxDevice::updateTxParams()
    {
        m_txInfo = m_rcShell->device(m_rxID, m_txID);
        if (m_txInfo)
        {
            /// @note async
            m_rcShell->updateTxParams(m_txInfo);
        }
    }

    unsigned RxDevice::dev2id(const std::string& nm)
    {
        std::string name = nm;
        name.erase( name.find( DEVICE_PATTERN ), strlen( DEVICE_PATTERN ) );

        return str2id(name);
    }

    unsigned RxDevice::str2id(const std::string& name)
    {
        unsigned num;
        std::stringstream ss;
        ss << name;
        ss >> num;
        return num;
    }

    std::string RxDevice::id2str(unsigned id)
    {
        return num2str(id);
    }
}
