#include "camera_pool.h"

#include <memory>

#include <nx/kit/utils.h>
#include <nx/network/nettools.h>
#include <nx/vms/testcamera/test_camera_ini.h>

#include "logger.h"
#include "frame_logger.h"
#include "file_cache.h"
#include "camera_options.h"
#include "camera_request_processor.h"
#include "camera_discovery_listener.h"

namespace nx::vms::testcamera {

CameraPool::CameraPool(
    const FileCache* const fileCache,
    QStringList localInterfacesToListen,
    QnCommonModule* commonModule,
    bool noSecondaryStream,
    int fpsPrimary,
    int fpsSecondary)
    :
    QnTcpListener(
        commonModule,
        localInterfacesToListen.isEmpty()
            ? QHostAddress::Any
            : QHostAddress(localInterfacesToListen[0]),
        ini().mediaPort
    ),
    m_fileCache(fileCache),
    m_localInterfacesToListen(std::move(localInterfacesToListen)),
    m_logger(new Logger("CameraPool")),
    m_frameLogger(new FrameLogger()),
    m_noSecondaryStream(noSecondaryStream),
    m_fpsPrimary(fpsPrimary),
    m_fpsSecondary(fpsSecondary)
{
}

CameraPool::~CameraPool()
{
}

void CameraPool::reportAddingCameras(
    bool cameraForEachFile,
    const CameraOptions& cameraOptions,
    int count,
    const QStringList& primaryFileNames,
    const QStringList& secondaryFileNames)
{
    if (!NX_ASSERT(!primaryFileNames.empty()))
        return;
    if (!m_noSecondaryStream && !NX_ASSERT(!secondaryFileNames.empty()))
        return;

    QString filesMessagePart = "\n    Primary stream file(s):";

    for (const QString& filename: primaryFileNames)
        filesMessagePart += "\n        " + filename;

    if (!m_noSecondaryStream)
    {
        filesMessagePart += "\n    Secondary stream file(s):";
        for (const QString& filename: secondaryFileNames)
            filesMessagePart += "\n        " + filename;
    }

    const QString cameraCountMessagePart = cameraForEachFile
        ? lm("%1 (one per each file)").args(primaryFileNames.size()).toQString()
        : QString::number(count);

    const QString offlineFreqMessagePart = (cameraOptions.offlineFreq == 0)
        ? ""
        : lm(", offlineFreq=%1%%").args(cameraOptions.offlineFreq);

    NX_LOGGER_INFO(m_logger, "Adding %1 camera(s)%2:%3",
        cameraCountMessagePart, offlineFreqMessagePart, filesMessagePart);
}

bool CameraPool::addCamera(
    const CameraOptions& cameraOptions,
    QStringList primaryFileNames,
    QStringList secondaryFileNames)
{
    const QStringList actualSecondaryFileNames =
        m_noSecondaryStream ? QStringList() : secondaryFileNames;

    auto camera = std::make_unique<Camera>(
        m_frameLogger.get(),
        m_fileCache,
        (int) m_cameraByMacAddress.size() + 1,
        cameraOptions,
        primaryFileNames,
        actualSecondaryFileNames);

    const auto macAddress = camera->macAddress();
    if (!NX_ASSERT(!macAddress.isNull()))
        return false;
    const auto [_, success] = m_cameraByMacAddress.insert({macAddress, std::move(camera)});
    if (!NX_ASSERT(success, "Unable to add camera: duplicate MAC %1.", macAddress))
        return false;

    m_cameraDiscoveryResponseByMacAddress.insert(
        {macAddress, std::make_shared<CameraDiscoveryResponse>(macAddress)});

    return true;
}

bool CameraPool::addCameraSet(
    const FileCache* fileCache,
    bool cameraForEachFile,
    const CameraOptions& cameraOptions,
    int count,
    const QStringList& primaryFileNames,
    const QStringList& secondaryFileNames)
{
    QMutexLocker lock(&m_mutex);

    reportAddingCameras(
        cameraForEachFile, cameraOptions, count, primaryFileNames, secondaryFileNames);

    if (!cameraForEachFile)
    {
        for (int i = 0; i < count; ++i)
        {
            if (!addCamera(
                cameraOptions, primaryFileNames, secondaryFileNames))
            {
                return false;
            }
        }
    }
    else // Run one camera for each primary-secondary pair of video files.
    {
        if (!NX_ASSERT(primaryFileNames.size() == secondaryFileNames.size()))
            return false;

        for (int i = 0; i < primaryFileNames.size(); i++)
        {
            if (!addCamera(
                cameraOptions, {primaryFileNames[i]}, {secondaryFileNames[i]}))
            {
                return false;
            }
        }
    }

    return true;
}

QByteArray CameraPool::obtainDiscoveryResponseMessage() const
{
    QMutexLocker lock(&m_mutex);

    DiscoveryResponse discoveryResponse(ini().mediaPort);
    for (const auto& [macAddress, camera]: m_cameraByMacAddress)
    {
        if (camera->isEnabled())
        {
            const auto it = m_cameraDiscoveryResponseByMacAddress.find(macAddress);
            if (!NX_ASSERT(it != m_cameraDiscoveryResponseByMacAddress.end()))
                continue;
            discoveryResponse.addCameraDiscoveryResponse(it->second);
        }
    }
    return discoveryResponse.makeDiscoveryResponseMessage();
}

bool CameraPool::startDiscovery()
{
    m_discoveryListener = std::make_unique<CameraDiscoveryListener>(
        m_logger.get(),
        [this]() { return obtainDiscoveryResponseMessage(); },
        m_localInterfacesToListen);

    if (!m_discoveryListener->initialize())
        return false;

    m_discoveryListener->start();
    base_type::start();

    return true;
}

QnTCPConnectionProcessor* CameraPool::createRequestProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket)
{
    QMutexLocker lock(&m_mutex);

    return new CameraRequestProcessor(
        this, std::move(clientSocket), m_noSecondaryStream, m_fpsPrimary, m_fpsSecondary);
}

Camera* CameraPool::findCamera(const nx::utils::MacAddress& macAddress) const
{
    QMutexLocker lock(&m_mutex);

    const auto& it = m_cameraByMacAddress.find(macAddress);
    if (it != m_cameraByMacAddress.end())
        return it->second.get();
    return nullptr;
}

} // namespace nx::vms::testcamera
