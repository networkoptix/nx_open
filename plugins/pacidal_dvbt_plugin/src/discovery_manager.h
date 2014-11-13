#ifndef ITE_DISCOVERY_MANAGER_H
#define ITE_DISCOVERY_MANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "camera_manager.h"
#include "rc_com.h"
#include "rc_shell.h"

namespace ite
{
    //!
    class DiscoveryManager : public nxcip::CameraDiscoveryManager
    {
        DEF_REF_COUNTER

    public:
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

        //

        static nxpt::CommonRefManager * refManager() { return &(Instance->m_refManager); }

    private:
        typedef std::shared_ptr<CameraManager> CameraPtr;

        static DiscoveryManager * Instance;

        mutable std::mutex m_contentMutex;
        std::map<unsigned, RxDevicePtr> m_rxDevs;   // {RxID, RxDev}
        std::multimap<unsigned, IDsLink> m_txLinks; // {TxID, RxTxLink}
        std::map<unsigned, CameraPtr> m_cameras;    // {CamId, Camera}
        ComPort comPort_;
        RCShell rcShell_;

        static void getRxDevNames(std::vector<std::string>& names);
        void updateRxDevices();
        void updateTxLinks();

        RxDevicePtr getRx4Tx(unsigned txID);
    };
}

#endif // ITE_DISCOVERY_MANAGER_H
