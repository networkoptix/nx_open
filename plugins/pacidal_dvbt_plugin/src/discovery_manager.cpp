#include "camera_manager.h"
#include "discovery_manager.h"

#include <dirent.h>
#include <string>
#include <set>

//#include <utils/common/log.h>

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

namespace
{
    static const char* VENDOR_NAME = "it930x";
#if 0
    void debugPrintInfo(const nxcip::CameraInfo& info)
    {
        std::string s = std::string(info.url);
        s += " - ";
        s += std::string(info.modelName);
        s += " - ";
        s += std::string(info.firmware);
        NX_LOG( s.c_str(), cl_logDEBUG1 );
    }
#endif
}

namespace ite
{
    DEFAULT_REF_COUNTER(DiscoveryManager)

    DiscoveryManager * DiscoveryManager::Instance;

    DiscoveryManager::DiscoveryManager()
    :   m_refManager( this ),
        comPort_("/dev/tnt0", 9600)
    {
        ComPort::Instance = &comPort_;
        rcShell_.startRcvThread();

        Instance = this;
    }

    DiscoveryManager::~DiscoveryManager()
    {
        for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it)
            it->second->releaseRef();

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

    void DiscoveryManager::getVendorName( char* buf ) const
    {
        strcpy( buf, VENDOR_NAME );
    }

    int DiscoveryManager::findCameras(nxcip::CameraInfo * cameras, const char * /*localInterfaceIPAddr*/)
    {
        if (! ComPort::Instance->isOK())
            return nxcip::NX_IO_ERROR;

        // FIXME
        static bool firstTime = true;
        if (firstTime)
        {
            firstTime = false;
            updateRxDevices();
            updateTxLinks();
        }

        unsigned cameraNum = 0;
        std::set<unsigned> foundTx;

        for (auto it = m_txLinks.begin(); it != m_txLinks.end(); ++it)
        {
            unsigned txID = it->second.txID;
            if (foundTx.find(txID) != foundTx.end())
                continue;

            std::string strTxID = RxDevice::id2str(txID);

            nxcip::CameraInfo& info = cameras[cameraNum];

            std::string strURL = strTxID + "-" + RxDevice::id2str(it->second.frequency);
            memset( &info, 0, sizeof(nxcip::CameraInfo) );
            strncpy( info.url, strURL.c_str(), std::min(strURL.size(), sizeof(nxcip::CameraInfo::url)-1) );
            strncpy( info.uid, strTxID.c_str(), std::min(strTxID.size(), sizeof(nxcip::CameraInfo::uid)-1) );

            CameraManager * cam = nullptr;

            {
                std::lock_guard<std::mutex> lock( m_contentMutex ); // LOCK

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
                //cam->getCameraInfo( &info );
                ++cameraNum;
            }
        }

        return cameraNum;
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

    nxcip::BaseCameraManager * DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
    {
        unsigned txID = RxDevice::str2id(info.uid);
        CameraManager* cam = nullptr;

        {
            std::lock_guard<std::mutex> lock( m_contentMutex ); // LOCK

            auto it = m_cameras.find(txID);
            if (it != m_cameras.end())
            {
                cam = it->second.get();
                if (!cam->rxDevice())
                    cam->setRxDevice( getRx4Tx(cam->txID()) );
            }


            // before findCameras() on restart, use saved info from DB
            if (!cam)
            {
                auto sp = std::make_shared<CameraManager>( info );  // create CameraManager
                m_cameras[txID] = sp;
                cam = sp.get();
                cam->addRef();
            }
        }

        if (cam && cam->rxDevice() && !cam->hasEncoders())
            cam->reload();

        if (cam)
            cam->addRef();
        return cam;
    }

    int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* count )
    {
        *count = 0;
        return nxcip::NX_NO_ERROR;
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

            std::lock_guard<std::mutex> lock( m_contentMutex ); // LOCK

            auto it = m_rxDevs.find(id);
            if (it == m_rxDevs.end())
            {
                m_rxDevs[id] = std::make_shared<RxDevice>(id);
            }
        }
    }

    // TODO: reduce time
    void DiscoveryManager::updateTxLinks()
    {
        std::vector<IDsLink> links;
        RxDevicePtr rxDev;

        for (size_t i = 0; i < m_rxDevs.size(); ++i)
        {
            {
                std::lock_guard<std::mutex> lock( m_contentMutex ); // LOCK

                rxDev = m_rxDevs[i];
            }

            if (rxDev->isLocked())
            {
                rcShell_.getDevIDsActivity(links);
            }
            else
            {
                rxDev->open();

                for (size_t j = 0; j < DeviceInfo::CHANNELS_NUM; ++j)
                {
                    // TODO: self device mutex
                    std::lock_guard<std::mutex> lock( m_contentMutex ); // LOCK

                    rxDev->lockF( DeviceInfo::chanFrequency(j) );
                    rcShell_.getDevIDsActivity(links);
                    rxDev->unlockF();
                }

                rxDev->close();
            }
        }

        std::lock_guard<std::mutex> lock( m_contentMutex ); // LOCK

        for (size_t i = 0; i < links.size(); ++i)
        {
            unsigned txID = links[i].txID;

            auto it = m_txLinks.find(txID);
            if (it == m_txLinks.end() || it->second.rxID != links[i].rxID)
                m_txLinks.insert( std::make_pair(txID, links[i]) );
        }
    }

    RxDevicePtr DiscoveryManager::getRx4Tx(unsigned txID)
    {
        // already ander lock: createCameraManager (L) -> reload -> getRx4Tx

        //std::lock_guard<std::mutex> lock( m_contentMutex ); // LOCK

        auto range = m_txLinks.equal_range(txID);
        for (auto it = range.first; it != range.second; ++it)
        {
            auto itRx = m_rxDevs.find(it->second.rxID);
            RxDevicePtr dev = itRx->second;

            if (dev->isLocked()) // device is reading another channel
                continue;

            if (!dev->isOpen())
                dev->open();
            dev->lockF(it->second.frequency);
            return dev;
        }

        return RxDevicePtr();
    }
}
