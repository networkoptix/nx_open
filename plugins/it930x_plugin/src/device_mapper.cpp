#include <dirent.h>

#include <memory>
#include <string>
#include <atomic>
#include <sstream>
#include <thread>

#include "device_mapper.h"

namespace ite
{
    class RC_DiscoveryThread;
    static RC_DiscoveryThread * updateThreadObj = nullptr;
    static std::thread updateThread;

    class RC_DiscoveryThread
    {
    public:
        RC_DiscoveryThread(DeviceMapper * devMapper)
        :   m_stopMe(false),
            m_devMapper(devMapper)
        {}

        RC_DiscoveryThread(const RC_DiscoveryThread& rct)
        :   m_stopMe(false),
            m_devMapper(rct.m_devMapper)
        {}

        void operator () ()
        {
            //static std::chrono::seconds s1(1);
            static std::chrono::seconds s10(10);

            updateThreadObj = this;

            std::this_thread::sleep_for(s10);

            while (!m_stopMe)
            {
                m_devMapper->updateRxDevices();

                for (size_t ch = 0; ch < TxDevice::CHANNELS_NUM; ++ch)
                {
                    // check restored devices
                    {
                        std::vector<DeviceMapper::DevLink> restored;
                        m_devMapper->getRestored(restored);

                        for (size_t i = 0; i < restored.size(); ++i)
                            m_devMapper->checkLink(restored[i]);
                    }

                    m_devMapper->updateTxDevices(ch);
                }
            }

            updateThreadObj = nullptr;
        }

        void stop() { m_stopMe = true; }

    private:
        std::atomic_bool m_stopMe;
        DeviceMapper * m_devMapper;
    };

    //

    DeviceMapper::DeviceMapper()
    {
        try
        {
            updateThread = std::thread( RC_DiscoveryThread(this) );
        }
        catch (std::system_error& )
        {}
    }

    DeviceMapper::~DeviceMapper()
    {
        if (updateThreadObj)
        {
            updateThreadObj->stop();
            updateThread.join();
        }
    }

    void DeviceMapper::getRx4Tx(unsigned short txID, std::vector<RxDevicePtr>& devs) const
    {
        devs.clear();

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        auto range = m_devLinks.equal_range(txID);
        for (auto itTx = range.first; itTx != range.second; ++itTx)
        {
            auto itRx = m_rxDevs.find(itTx->second.rxID);
            if (itRx != m_rxDevs.end())
            {
                RxDevicePtr dev = itRx->second;
                devs.push_back(dev);
            }
        }
    }

    void DeviceMapper::getRxDevNames(std::vector<std::string>& devs)
    {
        devs.clear();

        DIR * dir = ::opendir( "/dev" );
        if (dir != NULL)
        {
            struct dirent * ent;
            while ((ent = ::readdir( dir )) != NULL)
            {
                std::string file( ent->d_name );
                if (file.find( RxDevice::DEVICE_PATTERN ) != std::string::npos)
                    devs.push_back(file);
            }
            ::closedir( dir );
        }
    }

    void DeviceMapper::updateRxDevices()
    {
        std::vector<std::string> names;
        getRxDevNames(names);

        std::map<unsigned short, RxDevicePtr> rxDevs;

        for (size_t i = 0; i < names.size(); ++i)
        {
            unsigned rxID = RxDevice::dev2id(names[i]);

            auto it = m_rxDevs.find(rxID);
            if (it != m_rxDevs.end())
                rxDevs[rxID] = it->second;
            else
                rxDevs[rxID] = std::make_shared<RxDevice>(rxID); // create RxDevice
        }

        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            m_rxDevs.swap(rxDevs);
        }
    }

    void DeviceMapper::updateTxDevices(unsigned chan)
    {
        std::vector<RxDevicePtr> scanDevs;

        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            for (auto it = m_rxDevs.begin(); it != m_rxDevs.end(); ++it)
            {
                if (it->second.get() && ! it->second->isLocked())
                    scanDevs.push_back(it->second);
            }
        }

        // TODO: parallelize
        unsigned freq = TxDevice::chanFrequency(chan);
        for (size_t i = 0; i < scanDevs.size(); ++i)
        {
            DevLink link;
            link.rxID = scanDevs[i]->rxID();
            link.frequency = freq;
            checkLink(scanDevs[i], link);
        }
    }

    // THINK: could check TxID changes here
    void DeviceMapper::checkLink(RxDevicePtr dev, DevLink& link)
    {
        if (dev->findTx(link.frequency, link.txID))
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            addTxDevice(link);
            addDevLink(link);
        }
    }

    void DeviceMapper::checkLink(DevLink& link)
    {
        if (link.frequency == 0)
            return;

        RxDevicePtr dev;

        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            auto it = m_rxDevs.find(link.rxID);
            if (it != m_rxDevs.end())
                dev = it->second;
        }

        if (dev)
            checkLink(dev, link);
    }

    void DeviceMapper::addTxDevice(const DevLink& link)
    {
        // under lock

        auto it = m_txDevs.find(link.txID);
        if (it == m_txDevs.end())
            m_txDevs[link.txID] = std::make_shared<TxDevice>(link.txID, link.frequency);
        else
            it->second->setFrequency(link.frequency);
    }

    void DeviceMapper::addDevLink(const DevLink& link)
    {
        // under lock

        auto itNew = m_devLinks.end();
        auto range = m_devLinks.equal_range(link.txID);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second.rxID == link.rxID)
                itNew = it;
            else
                it->second.frequency = link.frequency;
        }

        if (itNew == m_devLinks.end())
            m_devLinks.insert( std::make_pair(link.txID, link) );
    }

    void DeviceMapper::txDevs(std::vector<TxDevicePtr>& txDevs) const
    {
        txDevs.clear();

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        txDevs.reserve(m_txDevs.size());
        for (auto it = m_txDevs.begin(); it != m_txDevs.end(); ++it)
            txDevs.push_back(it->second);
    }

    unsigned DeviceMapper::getFreq4Tx(unsigned short txID) const
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        auto it = m_txDevs.find(txID);
        if (it != m_txDevs.end())
            return it->second->frequency();
        return 0;
    }

    void DeviceMapper::restoreCamera(const nxcip::CameraInfo& info)
    {
        DevLink link;

        std::vector<unsigned short> rxIDs;
        DeviceMapper::parseInfo(info, link.txID, link.frequency, rxIDs);

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        for (auto it = rxIDs.begin(); it != rxIDs.end(); ++it)
        {
            link.rxID = *it;
            m_restore.push_back(link);
        }
    }

    void DeviceMapper::getRestored(std::vector<DevLink>& links)
    {
        links.clear();

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        links.swap(m_restore);
    }

    // CameraInfo stuff

    void DeviceMapper::makeInfo(nxcip::CameraInfo& info, unsigned short txID)
    {
        std::vector<RxDevicePtr> rxDevs;
        getRx4Tx(txID, rxDevs);

        std::vector<unsigned short> rxIDs;
        for (auto it = rxDevs.begin(); it != rxDevs.end(); ++it)
            rxIDs.push_back((*it)->rxID());

        unsigned freq = getFreq4Tx(txID);
        makeInfo(info, txID, freq, rxIDs);
    }

    void DeviceMapper::makeInfo(nxcip::CameraInfo& info, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs)
    {
        std::string strTxID = RxDevice::id2str(txID);

        memset( &info, 0, sizeof(nxcip::CameraInfo) );
        strncpy( info.modelName, "Pacidal", sizeof(nxcip::CameraInfo::modelName)-1 ); // TODO
        strncpy( info.url, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::url)-1) );
        strncpy( info.uid, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::uid)-1) );

        updateInfoAux(info, txID, frequency, rxIDs);
    }

    void DeviceMapper::updateInfoAux(nxcip::CameraInfo& info, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs)
    {
        std::stringstream ss;
        ss << txID << ' ' << frequency;

        for (size_t i = 0; i < rxIDs.size(); ++i)
            ss << ' ' << rxIDs[i];

        //memset(&info.auxiliaryData, 0, sizeof(nxcip::CameraInfo::auxiliaryData));
        unsigned len = std::min(ss.str().size(), sizeof(nxcip::CameraInfo::auxiliaryData)-1);
        strncpy(info.auxiliaryData, ss.str().c_str(), len);
        info.auxiliaryData[len] = 0;
    }

    void DeviceMapper::parseInfo(const nxcip::CameraInfo& info, unsigned short& txID, unsigned& frequency, std::vector<unsigned short>& outRxIDs)
    {
        if (info.auxiliaryData[0])
        {
            std::stringstream ss;
            ss << std::string(info.auxiliaryData);
            ss >> txID >> frequency;

            while (ss.rdbuf()->in_avail())
            {
                unsigned short rxID;
                ss >> rxID;
                outRxIDs.push_back(rxID);
            }
        }
    }
}
