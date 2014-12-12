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
        m_strength(0),
        m_quality(0),
        m_present(false)
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
        {}

        return false;
    }

    bool RxDevice::setChannel(unsigned short txID, unsigned chan)
    {
        if (isLocked())
            return m_rcShell->setChannel(rxID(), txID, chan);
        return false;
    }

    bool RxDevice::lockCamera(unsigned short txID)
    {
        if (!txID)
            return false;

        unsigned freq = m_rcShell->lastTxFrequency(txID);
        if (!freq)
            return false;

        if (tryLockF(freq))
        {
            m_txID = txID;
            m_txInfo = m_rcShell->device(m_rxID, m_txID);
            if (m_txInfo)
                m_txInfo->deviceInfo(m_txDriverInfo);
            return true;
        }

        return false;
    }

    bool RxDevice::tryLockF(unsigned freq)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        // prevent double locking at all (even with the same frequency)
        if (isLockedUnsafe())
            return false;

        try
        {
            m_device->lockChannel(freq);
            m_frequency = freq;

            stats();
            return true;
        }
        catch (const char * msg)
        {}

        return false;
    }

    void RxDevice::unlockF()
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

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
        try
        {
            m_quality = 0;
            m_strength = 0;
            m_present = false;
            bool locked = false;
            m_device->statistic(m_quality, m_strength, m_present, locked);
            return true;
        }
        catch (const char * msg)
        {}

        return false;
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
