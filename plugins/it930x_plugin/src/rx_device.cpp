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
        // HACK: close device if it's open
        m_device.reset(new It930x(m_rxID));
        m_device.reset(); // dtor
    }

    bool RxDevice::open()
    {
        try
        {
            m_devStream.reset();
            m_device.reset(); // dtor first
            m_device.reset(new It930x(m_rxID));
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

        if (lockF(freq))
        {
            m_txID = txID;
            return true;
        }

        return false;
    }

    bool RxDevice::lockF(unsigned freq)
    {
        try
        {
            if (isOpen())
                unlockF();
            else
                open();

            if (! m_device)
                return false;

            m_device->lockChannel(freq);
            m_devStream.reset( new It930Stream( *m_device ) );
            m_frequency = freq;

            stats();
            return true;
        }
        catch (const char * msg)
        {}

        return false;
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

    void RxDevice::driverInfo(std::string& driverVersion, std::string& fwVersion, std::string& company, std::string& model) const
    {
        if (m_device)
        {
            char verDriver[32] = "";
            char verAPI[32] = "";
            char verFWLink[32] = "";
            char verFWOFDM[32] = "";
            char xCompany[32] = "";
            char xModel[32] = "";

            m_device->info(verDriver, verAPI, verFWLink, verFWOFDM, xCompany, xModel);

            driverVersion = "Driver: " + std::string(verDriver) + " API: " + std::string(verAPI);
            fwVersion = "FW: " + std::string(verFWLink) + " OFDM: " + std::string(verFWOFDM);
            company = std::string(xCompany);
            model = std::string(xModel);
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
