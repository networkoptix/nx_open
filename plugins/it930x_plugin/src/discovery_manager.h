#ifndef ITE_DISCOVERY_MANAGER_H
#define ITE_DISCOVERY_MANAGER_H

#include <memory>
#include <map>
#include <mutex>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "device_mapper.h"

namespace ite
{
    class CameraManager;

    //!
    class DiscoveryManager : public nxcip::CameraDiscoveryManager, public ObjectCounter<DiscoveryManager>
    {
        DEF_REF_COUNTER

    public:
        typedef std::shared_ptr<CameraManager> CameraPtr;

        DiscoveryManager();
        virtual ~DiscoveryManager();

        // nxcip::CameraDiscoveryManager

        virtual void getVendorName( char* buf ) const override;
        virtual int findCameras( nxcip::CameraInfo* cameras, const char* localInterfaceIPAddr ) override;
        virtual int checkHostAddress( nxcip::CameraInfo* cameras, const char* address, const char* login, const char* password ) override;
        virtual int fromMDNSData(
            const char* discoveredAddress,
            const unsigned char* mdnsResponsePacket,
            int mdnsResponsePacketSize,
            nxcip::CameraInfo* cameraInfo ) override;
        virtual int fromUpnpData( const char* upnpXMLData, int upnpXMLDataSize, nxcip::CameraInfo* cameraInfo ) override;
        virtual nxcip::BaseCameraManager* createCameraManager( const nxcip::CameraInfo& info ) override;
        virtual int getReservedModelList( char** modelList, int* count ) override;

    private:
        static DiscoveryManager * Instance;

        mutable std::mutex m_mutex;
        DeviceMapper m_devMapper;
        std::map<unsigned short, CameraPtr> m_cameras; // {TxID, Camera}
    };
}

#endif // ITE_DISCOVERY_MANAGER_H
