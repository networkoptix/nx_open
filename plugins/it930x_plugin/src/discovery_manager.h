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
#include "rc_shell.h"

namespace ite
{
    //!
    class DiscoveryManager : public nxcip::CameraDiscoveryManager
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

        //

        static nxpt::CommonRefManager * refManager() { return &(Instance->m_refManager); }
        static DiscoveryManager * instance() { return Instance; }

        static void makeInfo(nxcip::CameraInfo& cameraInfo, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs);
        static void updateInfoAux(nxcip::CameraInfo& cameraInfo, unsigned short txID, unsigned frequency, const std::vector<unsigned short>& rxIDs);
        static void parseInfo(const nxcip::CameraInfo& cameraInfo, unsigned short& txID, unsigned& frequency, std::vector<unsigned short>& outRxIDs);

        // public for RC update devs thread
        void updateRxDevices();
        void updateTxLinks(unsigned chan);
        void updateOneTxLink(RxDevicePtr dev, unsigned frequency);

        void getRestored(std::vector<CameraPtr>& out)
        {
            std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

            out.swap(m_restored);
        }

        RxDevicePtr rxDev(unsigned short rxID) const
        {
            std::lock_guard<std::mutex> lock( m_mutex );

            auto it = m_rxDevs.find(rxID);
            if (it != m_rxDevs.end())
                return it->second;
            return RxDevicePtr();
        }

    private:
        static DiscoveryManager * Instance;

        mutable std::mutex m_mutex;
        std::map<unsigned short, CameraPtr> m_cameras;          // {TxID, Camera}
        std::map<unsigned short, RxDevicePtr> m_rxDevs;         // {RxID, RxDev}
        std::multimap<unsigned short, IDsLink> m_txLinks;       // {TxID, RxTxLink}
        std::vector<CameraPtr> m_restored;
        RCShell m_rcShell;

        void addTxLinks(const std::vector<IDsLink>&);
        void getRx4Tx(unsigned short txID, std::vector<RxDevicePtr>& out);

        static void getRxDevNames(std::vector<std::string>& names);
    };
}

#endif // ITE_DISCOVERY_MANAGER_H
