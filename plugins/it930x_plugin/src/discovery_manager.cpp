#include <dirent.h>

#include <cstdio>
#include <string>
#include <set>
#include <atomic>
#include <sstream>

#include "discovery_manager.h"
#include "camera_manager.h"

extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
    nxpl::PluginInterface * createNXPluginInstance()
    {
        return new ite::DiscoveryManager();
    }
}

namespace ite
{
    static const char * VENDOR_NAME = "ITE";

    class RC_DiscoveryThread;
    static RC_DiscoveryThread * updateThreadObj = nullptr;
    static std::thread updateThread;

    class RC_DiscoveryThread
    {
    public:
        RC_DiscoveryThread(DiscoveryManager * discovery, RCShell * shell)
        :   m_stopMe(false),
            m_discovery(discovery),
            m_rcShell(shell)
        {}

        RC_DiscoveryThread(const RC_DiscoveryThread& rct)
        :   m_stopMe(false),
            m_discovery(rct.m_discovery),
            m_rcShell(rct.m_rcShell)
        {}

        void operator () ()
        {
            static std::chrono::seconds s1(1);
            static std::chrono::seconds s10(10);

            updateThreadObj = this;

            std::this_thread::sleep_for(s10);

            while (!m_stopMe)
            {
                m_discovery->updateRxDevices();

                for (size_t ch = 0; ch < DeviceInfo::CHANNELS_NUM; ++ch)
                {
                    // check restored devices
                    {
                        std::vector<DiscoveryManager::CameraPtr> restored;
                        m_discovery->getRestored(restored);

                        for (size_t i = 0; i < restored.size(); ++i)
                        {
                            nxcip::CameraInfo info;
                            restored[i]->getCameraInfo(&info);

                            unsigned short txID;
                            unsigned frequency;
                            std::vector<unsigned short> rxIDs;
                            m_discovery->parseInfo(info, txID, frequency, rxIDs);

                            for (size_t i = 0; i < rxIDs.size(); ++i)
                            {
                                RxDevicePtr dev = m_discovery->rxDev(rxIDs[i]);
                                if (dev && frequency)
                                    m_discovery->updateOneTxLink(dev, frequency);
                            }
                        }
                    }

                    m_discovery->updateTxLinks(ch);
                }
            }

            updateThreadObj = nullptr;
        }

        void stop() { m_stopMe = true; }

    private:
        std::atomic_bool m_stopMe;
        DiscoveryManager * m_discovery;
        RCShell * m_rcShell;
    };

    //

    DEFAULT_REF_COUNTER(DiscoveryManager)

    DiscoveryManager * DiscoveryManager::Instance;

    DiscoveryManager::DiscoveryManager()
    :   m_refManager( this )
    {
        m_rcShell.startRcvThread();

        Instance = this;

        try
        {
            updateThread = std::thread( RC_DiscoveryThread(this, &m_rcShell) );
        }
        catch (std::system_error& )
        {}
    }

    DiscoveryManager::~DiscoveryManager()
    {
        for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it)
            it->second->releaseRef();

        if (updateThreadObj)
        {
            updateThreadObj->stop();
            updateThread.join();
        }

        Instance = nullptr;
    }

    void * DiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_CameraDiscoveryManager)
        {
            addRef();
            return this;
        }
        if (interfaceID == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }

        return nullptr;
    }

    int DiscoveryManager::checkHostAddress( nxcip::CameraInfo* /*cameras*/, const char* /*address*/, const char* /*login*/, const char* /*password*/ )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int DiscoveryManager::fromMDNSData(
        const char* /*discoveredAddress*/,
        const unsigned char* /*mdnsResponsePacket*/,
        int /*mdnsResponsePacketSize*/,
        nxcip::CameraInfo* /*cameraInfo*/ )
    {
        return nxcip::NX_NO_ERROR;
    }

    int DiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
    {
        return nxcip::NX_NO_ERROR;
    }

    int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
    {
        *count = 0;
        return nxcip::NX_NO_ERROR;
    }

    void DiscoveryManager::getVendorName( char* buf ) const
    {
        strcpy( buf, VENDOR_NAME );
    }

    int DiscoveryManager::findCameras(nxcip::CameraInfo * cameras, const char * /*localInterfaceIPAddr*/)
    {
        if (! ComPort::Instance->isOK())
            return nxcip::NX_IO_ERROR;

        std::vector<IDsLink> links;
        m_rcShell.getDevIDs(links);
        addTxLinks(links);

        unsigned cameraNum = 0;
        std::set<unsigned> foundTx;

        for (auto it = m_txLinks.begin(); it != m_txLinks.end(); ++it)
        {
            unsigned txID = it->second.txID;
            if (foundTx.find(txID) != foundTx.end())
                continue;

            nxcip::CameraInfo& info = cameras[cameraNum];
            CameraManager * cam = nullptr;

            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                auto itCam = m_cameras.find(txID);
                if (itCam == m_cameras.end())
                {
                    std::vector<unsigned short> rxIDs;
                    rxIDs.push_back(it->second.rxID);
                    makeInfo(info, txID, it->second.frequency, rxIDs);

                    auto sp = std::make_shared<CameraManager>( info ); // create CameraManager
                    m_cameras[txID] = sp;
                    cam = sp.get();
                    cam->addRef();
                }
                else
                    cam = itCam->second.get();
            }

            if (cam)
            {
                foundTx.insert(txID);

                // delayed receivers release
                if (cam->stopIfNeeds())
                    continue; // HACK: hide camera once

                std::vector<RxDevicePtr> rxs;
                getRx4Tx(cam->txID(), rxs);
                cam->addRxDevices(rxs);

                cam->getCameraInfo(&info);
                ++cameraNum;
            }
        }

        return cameraNum;
    }

    nxcip::BaseCameraManager * DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
    {
        unsigned short txID = RxDevice::str2id(info.uid);
        CameraManager * cam = nullptr;

        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            auto it = m_cameras.find(txID);
            if (it != m_cameras.end())
                cam = it->second.get();
        }

        // before findCameras() on restart, use saved info from DB
        if (!cam)
        {
            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                auto sp = std::make_shared<CameraManager>( info );  // create CameraManager
                m_cameras[txID] = sp;
                cam = sp.get();
                cam->addRef();

                m_restored.push_back(sp);
            }
        }

        if (cam)
            cam->addRef();
        return cam;
    }

    void DiscoveryManager::getRx4Tx(unsigned short txID, std::vector<RxDevicePtr>& out)
    {
        std::lock_guard<std::mutex> lock( m_mutex );

        auto range = m_txLinks.equal_range( txID );
        for (auto itTx = range.first; itTx != range.second; ++itTx)
        {
            auto itRx = m_rxDevs.find(itTx->second.rxID);
            if (itRx != m_rxDevs.end())
            {
                RxDevicePtr dev = itRx->second;
                out.push_back(dev);
            }
        }
    }

    void DiscoveryManager::getRxDevNames(std::vector<std::string>& devs)
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

    void DiscoveryManager::updateRxDevices()
    {
        std::vector<std::string> rxDevs;
        getRxDevNames(rxDevs);

        for (size_t i = 0; i < rxDevs.size(); ++i)
        {
            unsigned id = RxDevice::dev2id(rxDevs[i]);
#if 0
            if (id) // DEBUG: leave one Rx device
                continue;
#endif
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            auto it = m_rxDevs.find(id);
            if (it == m_rxDevs.end())
                m_rxDevs[id] = std::make_shared<RxDevice>(id, &m_rcShell); // create RxDevice

            // TODO: remove lost devices (USB)
        }
    }

    /// @warning It assumed that rxID from UNIX device name corresponds to rxID from Return Channel data
    void DiscoveryManager::updateTxLinks(unsigned chan)
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

        unsigned freq = DeviceInfo::chanFrequency(chan);

        for (size_t i = 0; i < scanDevs.size(); ++i)
            updateOneTxLink(scanDevs[i], freq);

        std::vector<IDsLink> links;
        m_rcShell.getDevIDs(links);
        addTxLinks(links);
    }

    void DiscoveryManager::updateOneTxLink(RxDevicePtr dev, unsigned freq)
    {
        if (dev->tryLockF(freq))
        {
            unsigned short rxID = dev->rxID();
            m_rcShell.setRxFrequency(rxID, freq); // frequency for new DeviceInfo

#if 1 // DEBUG
            printf("searching TxIDs - rxID: %d; frequency: %d; quality: %d; strength: %d; presence: %d\n",
                   dev->rxID(), freq, dev->quality(), dev->strength(), dev->present());
#endif
            // active devices communication
            if (dev->present() && dev->strength() > 0) // strength: 0..100
            {
                m_rcShell.sendGetIDs();
                usleep(DeviceInfo::SEND_WAIT_TIME_MS * 1000);
            }

            dev->unlockF();

            m_rcShell.setRxFrequency(dev->rxID(), 0);
        }
    }

    void DiscoveryManager::addTxLinks(const std::vector<IDsLink>& links)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        for (size_t i = 0; i < links.size(); ++i)
        {
            unsigned txID = links[i].txID;

            auto it = m_txLinks.find(txID);
            if (it == m_txLinks.end() || it->second.rxID != links[i].rxID)
            {
                m_txLinks.insert( std::make_pair(txID, links[i]) );
            }
            else
            {
                it->second.frequency = links[i].frequency;
                it->second.isActive = links[i].isActive;
            }
        }
    }

    // CameraInfo stuff

    void DiscoveryManager::makeInfo(nxcip::CameraInfo& info, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs)
    {
        std::string strTxID = RxDevice::id2str(txID);

        memset( &info, 0, sizeof(nxcip::CameraInfo) );
        strncpy( info.modelName, "Pacidal", sizeof(nxcip::CameraInfo::modelName)-1 ); // TODO
        strncpy( info.url, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::url)-1) );
        strncpy( info.uid, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::uid)-1) );

        updateInfoAux(info, txID, frequency, rxIDs);
    }

    void DiscoveryManager::updateInfoAux(nxcip::CameraInfo& info, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs)
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

    void DiscoveryManager::parseInfo(const nxcip::CameraInfo& info, unsigned short& txID, unsigned& frequency, std::vector<unsigned short>& outRxIDs)
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
