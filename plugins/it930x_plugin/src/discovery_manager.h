#ifndef ITE_DISCOVERY_MANAGER_H
#define ITE_DISCOVERY_MANAGER_H

#include <memory>
#include <map>
#include <mutex>
#include <atomic>
#include <list>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "rx_device.h"
#include "tx_device.h"

#include "timer.h"

namespace ite {

struct Settings
{
    int minStrength;
    int maxFramesPerSecond;
};

extern Settings settings;

class CameraManager;

// Helper class for synchronizing Rx -> Tx correspondence.
// In every moment we want to know if the given Rx is locked on the
// given Tx (for not locking two Rx's on the same Tx).
// This helper class provides thread safe interface for setting and getting
// this information.
class RxSyncManager
{
private:
    struct SyncInfo {
        int rxId;
        int txId;
        SyncInfo(int rxId, int txId)
            : rxId(rxId),
              txId(txId)
        {}
    };
    typedef std::list<SyncInfo> SyncInfoContainer;
    typedef SyncInfoContainer::iterator SyncInfoContainerIterator;

public:
    bool lock(int rxId, int txId);
    bool unlock(int txId);
    // returns rxId of the current camera owner or -1 if none found
    int isLocked(int txId);

private:
    template<typename Ret, typename FoundHandler, typename NotFoundHandler>
    Ret helper(FoundHandler fh, NotFoundHandler nfh, int txId)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        SyncInfoContainerIterator syncIt = std::find_if(
            m_syncInfo.begin(),
            m_syncInfo.end(),
            [txId](const SyncInfo &sinfo) {
                return sinfo.txId == txId;
            }
        );
        if (syncIt != m_syncInfo.cend()) {
            return fh(syncIt);
        } else {
            return nfh();
        }
    }

private:
    SyncInfoContainer m_syncInfo;
    std::mutex m_mutex;
};

// Helper class for managing hints for RxDevice::watchDogs.
// Represents correspondence RxId -> [(TxId, channel), ... , (TxId, channel)]
// Watchdogs may want to use this information to discover hinted
// cameras first.
class RxHintManager
{
public:
    struct HintInfo {
        int txId;
        int chan;
        bool used;
        HintInfo(int txId, int chan)
            : txId(txId),
              chan(chan),
              used(false)
        {}
        HintInfo(const HintInfo &other)
            : txId(other.txId),
              chan(other.chan),
              used(other.used)
        {}
    };
    typedef std::vector<HintInfo> HintInfoVector;
private:
    typedef std::map<int, HintInfoVector> RxToHintsMap;
public:
    void addHint(int rxId, int txId, int chan);
    // We use hint only once by one Rx since if
    // the same hint has been used before with no success
    // (or even successfully) it is actually no use to try once more
    // with another Rx.
    bool useHint(int rxId, int txId, int chan);
    HintInfoVector getHintsByRx(int rxId) const;
private:
    mutable std::mutex m_mutex;
    RxToHintsMap m_hints;
};

//!
class DiscoveryManager : public DefaultRefCounter<nxcip::CameraDiscoveryManager>
{
public:
//        typedef std::shared_ptr<CameraManager> CameraPtr;
    typedef CameraManager *CameraPtr;

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

//        virtual int notify( uint32_t event, uint32_t value, void * opt ) override;

    void findAndSwapCamerasRx(CameraPtr cam1, CameraPtr cam2);

private:
    // run once on first findCameras call
    void scan_2();
    // To speed up camera's going operational
    // we can try to search camera using Frequency, Rx and Tx Ids,
    // from camera info.
    // Speed up actually will take place only if we saved this camera
    // info at the prevous server launches.
    void makeHint(const nxcip::CameraInfo &info);

private:
    static DiscoveryManager * Instance;

    mutable std::mutex 			m_mutex;
    std::atomic_bool 			m_needScan;
    std::atomic_bool 			m_blockAutoScan;
    std::vector<CameraPtr> 		m_cameras;
    Timer 						m_rescanTimer;
    std::map<int, RxDevicePtr>	m_rxDevices;
};
} // namespace ite

#endif // ITE_DISCOVERY_MANAGER_H
