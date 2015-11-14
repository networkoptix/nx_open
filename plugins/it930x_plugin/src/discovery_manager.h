#ifndef ITE_DISCOVERY_MANAGER_H
#define ITE_DISCOVERY_MANAGER_H

#include <memory>
#include <map>
#include <mutex>
#include <atomic>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "rx_device.h"
#include "tx_device.h"

#include "timer.h"

namespace ite
{
    class CameraManager;

    //!
    class DiscoveryManager : public DefaultRefCounter<nxcip::CameraDiscoveryManager2>
    {
    public:
        typedef std::shared_ptr<CameraManager> CameraPtr;

        DiscoveryManager();
        virtual ~DiscoveryManager();

        // nxcip::PluginInterface

        virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;

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

        // nxcip::CameraDiscoveryManager2

        virtual int notify( uint32_t event, uint32_t value, void * opt ) override;

        void findAndSwapCamerasRx(CameraPtr cam1, CameraPtr cam2);

    private:
        static DiscoveryManager * Instance;

        mutable std::mutex m_mutex;
        std::atomic_bool m_needScan;
        std::atomic_bool m_blockAutoScan;
        std::vector<CameraPtr> m_cameras;
        Timer m_rescanTimer;

        int scan(bool isManual);

        // refactor

        std::vector<RxDevicePtr>  m_rxDevices;
        // run once on first findCameras call
        void scan_2();

    private:
        std::atomic<bool> m_firstFind;
    };
}

#endif // ITE_DISCOVERY_MANAGER_H
