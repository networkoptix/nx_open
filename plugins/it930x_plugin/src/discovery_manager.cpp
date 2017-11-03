#include <vector>
#include <cassert>
#include <future>
#include <string>
#include <sstream>
#include <algorithm>

#include <dirent.h>

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
extern RxHintManager rxHintManager;

// Sync Manager
bool RxSyncManager::lock(int rxId, int txId)
{
    return helper<bool>([](SyncInfoContainerIterator){return false;},
                        [this, rxId, txId](){
                            m_syncInfo.emplace_back(rxId, txId);
                            return true;
                        },
                        txId);
}

bool RxSyncManager::unlock(int txId)
{
    return helper<bool>([this](SyncInfoContainerIterator it){
                            m_syncInfo.erase(it);
                            return true;
                        },
                        [](){return false;},
                        txId);
}

int RxSyncManager::isLocked(int txId)
{
    return helper<int>([this](SyncInfoContainerIterator it){
                            return it->rxId;
                        },
                        [](){return -1;},
                        txId);
}

// Hint Manager
void RxHintManager::addHint(int rxId, int txId, int chan)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    auto &rxHints = m_hints[rxId];
    auto hintIt = std::find_if(rxHints.begin(), rxHints.end(),
                               [txId](const HintInfo &hinfo) {
                                   return hinfo.txId == txId;
                               });
    if (hintIt != rxHints.end()) {
        hintIt->chan = chan;
    } else {
        rxHints.emplace_back(txId, chan);
    }
}

bool RxHintManager::useHint(int rxId, int txId, int chan)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    auto &rxHints = m_hints[rxId];
    auto hintIt = std::find_if(rxHints.begin(), rxHints.end(),
                               [txId](const HintInfo &hinfo) {
                                   return hinfo.txId == txId;
                               });
    if (hintIt != rxHints.end()) {
        if (hintIt->chan != chan) {
            debug_printf("[RxHintManager::lockHint] hint channel (%d) \
                          and requested channel (%d) don't match\n",
                         hintIt->chan, chan);
            return false;
        } else if (hintIt->used) {
            debug_printf("[RxHintManager::lockHint] hint for txId (%d) \
                          channel (%d) already used\n",
                         txId, chan);
            return false;
        } else {
            hintIt->used = true;
            return true;
        }
    } else {
        debug_printf("[RxHintManager::lockHint] txId (%d) \
                      channel (%d) not found\n",
                     txId, chan);
        return false;
    }
}

RxHintManager::HintInfoVector RxHintManager::getHintsByRx(int rxId) const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    auto rxHintsIt = m_hints.find(rxId);
    if (rxHintsIt == m_hints.end()) {
        debug_printf("[RxHintManager::getHintsByRx] no hints for this Rx (%d)\n",
                     rxId);
        return HintInfoVector();
    } else {
        return rxHintsIt->second;
    }
}
} // namespace ite

namespace ite {

Settings settings;

namespace aux {

// ConfigParser ------------------------------------------------------------------------------------
class ConfigParser
{
public:
    ConfigParser(const std::string& fileName);
    int parseCount() const;
    bool hasFile() const;
private:
    static const std::string kMinStrengthKey;
    static const std::string kMaxFramesPerSecKey;

    int m_parsedCount = 0;
    bool m_hasFile = false;

    void parseIntVal(const std::string& valueString, int* value);
};

const std::string ConfigParser::kMinStrengthKey = "minStrength";
const std::string ConfigParser::kMaxFramesPerSecKey = "maxFramesPerSecond";


int ConfigParser::parseCount() const
{
    return m_parsedCount;
}

bool ConfigParser::hasFile() const
{
    return m_hasFile;
}

ConfigParser::ConfigParser(const std::string &fileName)
{
    std::ifstream ifs(fileName);
    if (!ifs.is_open())
        return;

    m_hasFile = true;
    std::string line;
    while (!ifs.eof() && !ifs.fail())
    {
        std::getline(ifs, line);
        auto delimeterPos = line.find('=');
        if (delimeterPos == std::string::npos || delimeterPos == line.size() - 1)
            continue;

        auto key = line.substr(0, delimeterPos);
        auto value = line.substr(delimeterPos + 1);

        if (strcasecmp(kMinStrengthKey.c_str(), key.c_str()) == 0)
        {
            parseIntVal(value, &settings.minStrength);
            std::cout << "ITE930_INFO: successfully parsed minStrength value: " << settings.minStrength
                      << std::endl;
        }
        else if (strcasecmp(kMaxFramesPerSecKey.c_str(), key.c_str()) == 0)
        {
            parseIntVal(value, &settings.maxFramesPerSecond);
            std::cout << "ITE930_INFO: successfully parsed maxFramesPerSecond value: "
                      << settings.maxFramesPerSecond << std::endl;
        }
    }
}

void ConfigParser::parseIntVal(const std::string &valueString, int *value)
{
    try
    {
        *value = std::stoi(valueString);
        ++m_parsedCount;
    }
    catch (...)
    {
        std::cout << "ITE930_ERROR: Failed to parse config value: " << valueString << std::endl;
    }
}
// -------------------------------------------------------------------------------------------------

// aux free functions ------------------------------------------------------------------------------
static const char* getPath(const char* fullName, char* buf, int size)
{
    int len = strlen(fullName);
    int i;
    int newSize = 0;

    for (i = len - 1; i >= 0; --i) {
        if (fullName[i] == '/') {
            newSize = i;
            break;
        }
    }

    for (i = 0; i < newSize && i < size; ++i) {
        buf[i] = fullName[i];
    }

    if (i < size - 1) {
        buf[i + 1] = '\0';
    }

    return buf;
}

static const char* exeName(char* buf, int size)
{
    if (readlink("/proc/self/exe", buf, size) < 0) {
        return NULL;
    }

    return buf;
}

static void parseConfigFile()
{
    const int kBufSize = 512;
    char buf[kBufSize];

    memset(buf, '\0', kBufSize);
    if (exeName(buf, kBufSize) == nullptr)
    {
        std::cout << "ITE930_WARNING: Failed to get exe path" << std::endl;
        return;
    }

    if (getPath(buf, buf, kBufSize) == nullptr)
    {
        std::cout << "ITE930_WARNING: Failed to extract path compononet from exe path" << std::endl;
        return;
    }

    strcat(buf, "it930.conf");
    std::cout << "ITE930_INFO: trying to use config file: " << buf << std::endl;

    ConfigParser parser("it930.conf");
    if (!parser.hasFile())
        return;

    if (parser.parseCount() != 0)
        return;

    std::cout << "ITE930_WARNING: No config settings have been parsed" << std::endl;
}
// -------------------------------------------------------------------------------------------------

} // namespace aux


// DiscoveryManager --------------------------------------------------------------------------------

const int RESCAN_TIMEOUT = 300 * 1000; // ms

DiscoveryManager * DiscoveryManager::Instance;

DiscoveryManager::DiscoveryManager()
:   m_needScan(true),
    m_blockAutoScan(false)
{
    Instance = this;
    aux::parseConfigFile();
    m_rescanTimer.restart();
}

DiscoveryManager::~DiscoveryManager()
{
    ITE_LOG() << FMT("~DiscoveryManager() called");
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        for (auto it = m_rxDevices.begin(); it != m_rxDevices.end(); ++it) {
            it->second->pleaseStop();
        }
    }
    // release CameraManagers before DeviceMapper
    for (auto it = m_cameras.begin(); it != m_cameras.end(); ++it)
        (*it)->releaseRef();

    Instance = nullptr;
}

void * DiscoveryManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
//        if (interfaceID == nxcip::IID_CameraDiscoveryManager2)
//        {
//            addRef();
//            return this;
//        }
    if (interfaceID == nxcip::IID_CameraDiscoveryManager)
    {
        addRef();
        return this;
    }
    else if (interfaceID == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }

    return nullptr;
}

void DiscoveryManager::getVendorName( char* buf ) const
{
    static const char * VENDOR_NAME = "it930x";
    strcpy( buf, VENDOR_NAME );
}

int DiscoveryManager::fromMDNSData(
    const char* /*discoveredAddress*/,
    const unsigned char* /*mdnsResponsePacket*/,
    int /*mdnsResponsePacketSize*/,
    nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int DiscoveryManager::fromUpnpData( const char* /*upnpXMLData*/, int /*upnpXMLDataSize*/, nxcip::CameraInfo* /*cameraInfo*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int DiscoveryManager::getReservedModelList( char** /*modelList*/, int* /*count*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

static const char * internal = "HACK";

int DiscoveryManager::checkHostAddress( nxcip::CameraInfo* cameras, const char* /*address*/, const char* /*login*/, const char* /*password*/ )
{
    return findCameras(cameras, internal);
}

int DiscoveryManager::findCameras(nxcip::CameraInfo * cameras, const char * opt)
{
    static const uint64_t kRescanTimeoutMs = 1000 * 120;
    if (m_needScan || m_rescanTimer.elapsedMS() > kRescanTimeoutMs)
    {
        m_needScan = false;
        scan_2();
        m_rescanTimer.restart();
    }

    int cameraCount = 0;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_rxDevices.begin(); it != m_rxDevices.end(); ++it)
    {
        debug_printf("[DiscoveryManager::findCameras] trying RX %d\n", (*it)->rxID());
        if (it->second->deviceReady())
        {
            debug_printf("[DiscoveryManager::findCameras] RX %d is good!\n", (*it)->rxID());
            nxcip::CameraInfo info = it->second->getCameraInfo();
            debug_printf(
                "[DiscoveryManager::findCameras] Rx: %d; cam uid: %s cam url: %s\n",
                (*it)->rxID(),
                info.uid,
                info.url
            );

            cameras[cameraCount++] = info;
        }
        else
        {
            debug_printf("[DiscoveryManager::findCameras] cam %d is not good!\n", (*it)->rxID());
        }
    }
    return cameraCount;
    // <refactor
}

void DiscoveryManager::findAndSwapCamerasRx(CameraPtr cam1, CameraPtr cam2)
{
    std::lock(cam1->get_mutex(), cam2->get_mutex());
    std::lock_guard<std::mutex> lk1(cam1->get_mutex(), std::adopt_lock);
    std::lock_guard<std::mutex> lk2(cam2->get_mutex(), std::adopt_lock);

    cam1->rxDeviceRef().swap(cam2->rxDeviceRef());
}

void DiscoveryManager::makeHint(const nxcip::CameraInfo &info)
{	// We can find camera by (rxId, txId, channel) unique key.
    // This information is stored in CameraInfo::auxillary data string.
    // Format is 'rxId,txId,chan'.
    std::stringstream ss(info.auxiliaryData);
    auto extractInt = [&ss]() {
        int tmp;
        try {
            while (1) {
                int c = ss.get();
                if (std::isdigit(c)) {
                    ss.unget();
                    break;
                } else if (ss.fail()) {
                    return -1;
                }
            }
            ss >> tmp;
        } catch (const std::exception &) {
            return -1;
        }
        if (ss.fail())
            return -1;
        return tmp;
    };

    int rxId = extractInt();
    int txId = extractInt();
    int freq = extractInt();

    if (rxId == -1 || txId == -1 || freq == -1)
    {	// Bailing out if any of the key data is absent
        debug_printf(
            "[DiscoveryManager::findCameraByInfo] failed to extract auxillary camera data, original string is %s\n",
            info.auxiliaryData
        );
        return;
    }
    rxHintManager.addHint(rxId, txId, TxDevice::chan4freq(freq));

    auto rxIt = m_rxDevices.find(rxId);
    if (rxIt == m_rxDevices.end())
    {
        RxDevicePtr newRxDevice(new RxDevice(rxId));
        newRxDevice->startWatchDog();
        m_rxDevices.emplace(rxId, newRxDevice);
    }

    return;
}

nxcip::BaseCameraManager * DiscoveryManager::createCameraManager( const nxcip::CameraInfo& info )
{
    std::lock_guard<std::mutex> lock(m_mutex);

    unsigned short txID = RxDevice::str2id(info.uid);
    CameraManager * cam = nullptr;
//        ITE_LOG() << FMT("[DiscoveryManager::createCameraManager] trying to get cam %d\n", txID);

    // First lets find out if there are any rxDev which is locked on this tx right now
    // and if there are any CameraManager objects that operates with this tx.
    auto cameraIt = std::find_if(m_cameras.begin(), m_cameras.end(), [txID](CameraPtr c) { return c->cameraId() == txID; });

    auto rxIt = std::find_if(m_rxDevices.begin(), m_rxDevices.end(),
        [txID](const std::pair<int, RxDevicePtr> &p)
        {
            return p.second->getTxDevice() && p.second->getTxDevice()->txID() == txID;
        }
    );

    // Second let's deal with Rx
    RxDevicePtr foundRxDev;

    if (rxIt == m_rxDevices.end())
    { 	// No camera found in already present cameras. But we have one option left.
        // Let's make a hint for rx watchdogs with caminfo. Maybe we've already
        // dicovered this camera earlier (on the previous mediaserver run).
        makeHint(info);
//            ITE_LOG() << FMT("[DiscoveryManager::createCameraManager] no camera found\n");
        if (cameraIt != m_cameras.end() && (*cameraIt)->rxDeviceRef())
        {	// Notify camera that its rx device is no longer operational
//                ITE_LOG() << FMT("[DiscoveryManager::createCameraManager] releasing cameraManager rx\n");
            std::lock_guard<std::mutex> lock((*cameraIt)->get_mutex());
            (*cameraIt)->rxDeviceRef().reset();
        }
        return nullptr;
    }
    else
    {
        foundRxDev = rxIt->second;
    }

    // Now we know if rx is operational. Let's deal with related cameraManager.
    if (cameraIt != m_cameras.end())
    {	// related camera found
        if ((*cameraIt)->rxDeviceRef())
        {	// camera has ref to some rx device
            if ((*cameraIt)->rxDeviceRef() == foundRxDev)
            {	// Good! Camera's rx and found from the pool match
//                    ITE_LOG() << FMT("[DiscoveryManager::createCameraManager] camera rxDevice already correct. Camera found!\n");
            }
            else
            {	// Camera has reference to some other rx device. This may be the case if cameras have been
                // physically replugged. Not critical though, we can just replace rxs.
//                    ITE_LOG() << FMT("[DiscoveryManager::createCameraManager] camera rxDevice is not correct, replacing. Camera found!\n");
                std::lock_guard<std::mutex> lock((*cameraIt)->get_mutex());
                (*cameraIt)->rxDeviceRef().reset();
                (*cameraIt)->rxDeviceRef() = foundRxDev;
                (*cameraIt)->rxDeviceRef()->setCamera((*cameraIt));
            }
            cam = *cameraIt;
            cam->addRef();
        }
        else
        {	// This can be if camera's been unplugged and then replugged to the same port
//                ITE_LOG() << FMT("[DiscoveryManager::createCameraManager] No rxDevice for camera. Setting one and returning. Camera found!\n");
            std::lock_guard<std::mutex> lock((*cameraIt)->get_mutex());
            (*cameraIt)->rxDeviceRef() = foundRxDev;
            (*cameraIt)->rxDeviceRef()->setCamera((*cameraIt));

            cam = *cameraIt;
            cam->addRef();
        }
    }
    else
    {	// This should occur once after launch. Creating brand new CameraManager.
//            ITE_LOG() << FMT("[DiscoveryManager::createCameraManager] creating new camera with rx device %d. Camera found!\n", (foundRxDev)->rxID());
        CameraPtr newCam(new CameraManager(foundRxDev));
        m_cameras.push_back(newCam);
        cam = newCam;
        cam->addRef();
    }

    return cam;
}

//    int DiscoveryManager::notify(uint32_t event, uint32_t value, void * )
//    {
//        assert(0);
//        switch (event)
//        {
//            case EVENT_SEARCH:
//            {
//                if (value == EVENT_SEARCH_NEW_SEARCH)
//                {
//                    //m_needScan = true;
//                    m_blockAutoScan = true;
//                }

//                return nxcip::NX_NO_ERROR;
//            }

//            default:
//                break;
//        }

//        return nxcip::NX_NO_ERROR;
//    }

void getRxDevNames(std::vector<std::string>& devs)
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

void DiscoveryManager::scan_2()
{
    debug_printf("[scan_2] started\n");

    // Getting available Rx devices
    // This is done once since they should not
    // be altered while server is up.
    std::vector<std::string> names;
    getRxDevNames(names);

    for (size_t i = 0; i < names.size(); ++i)
    {
        unsigned rxID = RxDevice::dev2id(names[i]);
        std::lock_guard<std::mutex> lk(m_mutex);

        auto rxIt = m_rxDevices.find(rxID);
        if (rxIt == m_rxDevices.end())
        {
            RxDevicePtr newRxDevice(new RxDevice(rxID));
            ITE_LOG() << FMT("Rx %d starting watchdog", rxID);

            newRxDevice->startWatchDog();

            ITE_LOG() << FMT("Rx %d watchdog started", rxID);
            m_rxDevices.emplace(rxID, newRxDevice);
        }
        else
        {
            RxDevicePtr rx = rxIt->second;
            if (!rx->running() && !rx->needStop())
            {   // Rx open failed at the last attempt. Retrying.
                ITE_LOG() << FMT("Retrying open Rx %d", rxID);
                rx->startWatchDog();
            }
        }
    }
}
// -------------------------------------------------------------------------------------------------

} // namespace ite
