#include <dirent.h>

#include <cstdio>
#include <string>
#include <set>
#include <atomic>

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

    class RCUpdateDevsThread;
    static RCUpdateDevsThread * updateThreadObj = nullptr;
    static std::thread updateThread;

    class RCUpdateDevsThread
    {
    public:
        RCUpdateDevsThread(DiscoveryManager * discovery, RCShell * shell)
        :   m_stopMe(false),
            m_discovery(discovery),
            m_rcShell(shell)
        {}

        RCUpdateDevsThread(const RCUpdateDevsThread& rct)
        :   m_stopMe(false),
            m_discovery(rct.m_discovery),
            m_rcShell(rct.m_rcShell)
        {}

        void operator () ()
        {
            static const unsigned WAIT_RC_TIMEOUT_S = 4;
            static std::chrono::seconds dura( WAIT_RC_TIMEOUT_S );
            static std::chrono::milliseconds ms10(10);

            updateThreadObj = this;

            while (!m_stopMe)
            {
                m_discovery->updateRxDevices();

                for (size_t ch = 0; ch < DeviceInfo::CHANNELS_NUM; ++ch)
                {
                    m_discovery->updateTxLinks(ch);
                    std::this_thread::sleep_for(ms10); // free devices for a while
                }

                std::this_thread::sleep_for(dura);
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
    :   m_refManager( this ),
        comPort_("/dev/tnt0", 9600)
    {
        ComPort::Instance = &comPort_;
        rcShell_.startRcvThread();

        Instance = this;

        try
        {
            updateThread = std::thread( RCUpdateDevsThread(this, &rcShell_) );
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
        rcShell_.getDevIDs(links);
        addTxLinks(links);

        unsigned cameraNum = 0;
        std::set<unsigned> foundTx;

        for (auto it = m_txLinks.begin(); it != m_txLinks.end(); ++it)
        {
            unsigned txID = it->second.txID;
            if (foundTx.find(txID) != foundTx.end())
                continue;

            nxcip::CameraInfo& info = cameras[cameraNum];
            makeInfo(info, txID, 0xffff, it->second.frequency);

            CameraManager * cam = nullptr;

            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                auto it = m_cameras.find(txID);
                if (it == m_cameras.end())
                {
                    auto sp = std::make_shared<CameraManager>( info ); // create CameraManager
                    m_cameras[txID] = sp;
                    cam = sp.get();
                    cam->addRef();
                }
                else
                    cam = it->second.get();
            }

            if (cam)
            {
                foundTx.insert(txID);

                // deplayed receivers release
                if (cam->stopIfNeeds())
                    continue; // HACK: hide camera once

                getRx4Camera(cam);

                cam->getCameraInfo( &info );
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
            unsigned short rxID;
            unsigned frequency;
            parseInfo(info, txID, rxID, frequency);

            //checkLink(txID, rxID, frequency);

            {
                std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

                auto sp = std::make_shared<CameraManager>( info );  // create CameraManager
                m_cameras[txID] = sp;
                cam = sp.get();
                cam->addRef();
            }
        }

        if (cam)
            cam->addRef();
        return cam;
    }

    void DiscoveryManager::getRx4Camera(CameraManager * cam)
    {
        std::vector<RxDevicePtr> rxs;
        getRx4Tx(cam->txID(), rxs);

        for (auto it = rxs.begin(); it != rxs.end(); ++it)
             cam->addRxDevice(*it);
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
                m_rxDevs[id] = std::make_shared<RxDevice>(id, &rcShell_); // create RxDevice

            // TODO: remove lost devices (USB)
        }
    }

    /// @warning It assumed that rxID from UNIX device name corresponds to rxID from Return Channel data
    void DiscoveryManager::updateTxLinks(unsigned chan)
    {
        std::vector<RxDevicePtr> scanDevs;

        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            for (size_t i = 0; i < m_rxDevs.size(); ++i)
            {
                if (m_rxDevs[i].get() && ! m_rxDevs[i]->isLocked())
                    scanDevs.push_back(m_rxDevs[i]);
            }
        }

        unsigned freq = DeviceInfo::chanFrequency(chan);

        for (size_t i = 0; i < scanDevs.size(); ++i)
        {
            std::lock_guard<std::mutex> lock( scanDevs[i]->mutex() ); // LOCK device

            if (scanDevs[i]->isLocked())
                continue;

            if (! scanDevs[i]->isOpen())
                scanDevs[i]->open();

            unsigned short rxID = scanDevs[i]->rxID();

            rcShell_.setRxFrequency(rxID, freq); // frequency for new devices
            scanDevs[i]->lockF(freq);

            if (scanDevs[i]->isLocked())
            {
#if 1           // DEBUG
                printf("searching TxIDs - rxID: %d; frequency: %d; strength: %d; presence: %d\n",
                       scanDevs[i]->rxID(), freq, scanDevs[i]->strength(), scanDevs[i]->present());
#endif
                // active devices communication
                if (scanDevs[i]->present() && scanDevs[i]->strength() > 0) // strength: 0..100
                {
                    rcShell_.sendGetIDs();
                    usleep(DeviceInfo::SEND_WAIT_TIME_MS * 1000);
                }

                scanDevs[i]->unlockF();
            }

            rcShell_.setRxFrequency(scanDevs[i]->rxID(), 0);
            //scanDevs[i]->close();
        }

        std::vector<IDsLink> links;
        rcShell_.getDevIDs(links);
        addTxLinks(links);
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

    // rxID = 0xffff means "no rxID set"
    void DiscoveryManager::makeInfo(nxcip::CameraInfo& info, unsigned short txID, unsigned short rxID, unsigned frequency)
    {
        std::string strTxID = RxDevice::id2str(txID);

        memset( &info, 0, sizeof(nxcip::CameraInfo) );
        strncpy( info.modelName, "Pacidal", sizeof(nxcip::CameraInfo::modelName)-1 ); // TODO
        strncpy( info.url, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::url)-1) );
        strncpy( info.uid, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::uid)-1) );

        unsigned shift = sizeof(unsigned short);
        memcpy(info.auxiliaryData, &txID, shift);
        memcpy(info.auxiliaryData + shift, &rxID, shift);
        memcpy(info.auxiliaryData + shift*2, &frequency, sizeof(unsigned));
    }

    void DiscoveryManager::updateInfo(nxcip::CameraInfo& info, unsigned short rxID, unsigned frequency)
    {
        unsigned shift = sizeof(unsigned short);
        //memcpy(info.auxiliaryData, &txID, shift);
        memcpy(info.auxiliaryData + shift, &rxID, shift);
        memcpy(info.auxiliaryData + shift*2, &frequency, sizeof(unsigned));
    }

    void DiscoveryManager::parseInfo(const nxcip::CameraInfo& info, unsigned short& txID, unsigned short& rxID, unsigned& frequency)
    {
        unsigned shift = sizeof(unsigned short);
        memcpy(&txID, info.auxiliaryData, shift);
        memcpy(&rxID, info.auxiliaryData + shift, shift);
        memcpy(&frequency, info.auxiliaryData + shift*2, sizeof(unsigned));
    }

    // Tx parameters

    /// @warning could fail if Tx moved to another Rx and RxID-TxID link still exists
    bool DiscoveryManager::setChannel(unsigned short txID, unsigned chan)
    {
        unsigned freq = DeviceInfo::chanFrequency(chan);

        std::vector<RxDevicePtr> rxs;
        getRx4Tx(txID, rxs);

        for (auto it = rxs.begin(); it != rxs.end(); ++it)
        {
            RxDevicePtr dev = *it;

            if (! dev->isLocked())
            {
                dev->lockF(freq);
                if (rcShell_.setChannel(dev->rxID(), txID, chan))
                    return true;
            }
        }

        return false;
    }
}
