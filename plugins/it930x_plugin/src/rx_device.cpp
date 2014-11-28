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
        m_rcShell(rc),
        m_camera(nullptr),
        m_frequency(0),
        m_strength(0),
        m_present(false)
    {}

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

    bool RxDevice::lockCamera(CameraManager * cam)
    {
        if (!cam)
            return false;

        unsigned freq = m_rcShell->lastTxFrequency(cam->txID());
        if (!freq)
            return false;

        if (lockF(freq))
        {
            m_camera = cam;
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

            if (! m_device.get())
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
            bool locked = false;
            uint8_t abortCount = 0;
            float ber = 0.0f;

            m_present = false;
            m_strength = 0;
            m_device->statistic(locked, m_present, m_strength, abortCount, ber);
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
