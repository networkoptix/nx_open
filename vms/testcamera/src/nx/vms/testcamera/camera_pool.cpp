#include "camera_pool.h"

#include <memory>

#include <nx/kit/utils.h>
#include <nx/network/nettools.h>
#include <core/resource/test_camera_ini.h>

#include "logger.h"
#include "frame_logger.h"
#include "file_cache.h"
#include "camera_options.h"
#include "camera_request_processor.h"
#include "camera_discovery_listener.h"

namespace nx::vms::testcamera {

CameraPool::CameraPool(
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
        testCameraIni().mediaPort
    ),
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
    const CameraOptions& testCameraOptions,
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

    const QString offlineFreqMessagePart = (testCameraOptions.offlineFreq == 0)
        ? ""
        : lm(", offlineFreq=%1%%").args(testCameraOptions.offlineFreq);

    NX_LOGGER_INFO(m_logger, "Adding %1 camera(s)%2:%3",
        cameraCountMessagePart, offlineFreqMessagePart, filesMessagePart);
}

bool CameraPool::addCamera(
    const FileCache* fileCache,
    const CameraOptions& testCameraOptions,
    QStringList primaryFileNames,
    QStringList secondaryFileNames)
{
    auto camera = std::make_unique<Camera>(
        m_frameLogger.get(),
        fileCache,
        (int) m_cameraByMac.size() + 1,
        testCameraOptions,
        primaryFileNames,
        m_noSecondaryStream ? QStringList() : secondaryFileNames);

    const auto [_, success] = m_cameraByMac.insert({camera->mac(), std::move(camera)});

    NX_ASSERT(success, "Unable to add camera with duplicate MAC %1.", camera->mac());
    return success;
}

bool CameraPool::addCameras(
    const FileCache* fileCache,
    bool cameraForEachFile,
    const CameraOptions& testCameraOptions,
    int count,
    const QStringList& primaryFileNames,
    const QStringList& secondaryFileNames)
{
    QMutexLocker lock(&m_mutex);

    reportAddingCameras(
        cameraForEachFile, testCameraOptions, count, primaryFileNames, secondaryFileNames);

    if (!cameraForEachFile)
    {
        for (int i = 0; i < count; ++i)
        {
            if (!addCamera(fileCache, testCameraOptions, primaryFileNames, secondaryFileNames))
                return false;
        }
    }
    else // Run one camera for each primary-secondary pair of video files.
    {
        if (!NX_ASSERT(primaryFileNames.size() == secondaryFileNames.size()))
            return false;

        for (int i = 0; i < primaryFileNames.size(); i++)
        {
            if (!addCamera(
                fileCache, testCameraOptions, {primaryFileNames[i]}, {secondaryFileNames[i]}))
            {
                return false;
            }
        }
    }
    return true;
}

bool CameraPool::startDiscovery()
{
    QByteArray discoveryResponseData = QByteArray::number(testCameraIni().mediaPort);

    for (const auto& [mac, camera]: m_cameraByMac)
    {
        if (camera->isEnabled())
        {
            discoveryResponseData.append(';');
            discoveryResponseData.append(mac);
        }
    }

    m_discoveryListener.reset(new CameraDiscoveryListener(
        m_logger.get(), discoveryResponseData, m_localInterfacesToListen));

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

Camera* CameraPool::findCamera(const QString& mac) const
{
    QMutexLocker lock(&m_mutex);

    const auto& it = m_cameraByMac.find(mac);
    if (it != m_cameraByMac.end())
        return it->second.get();
    return nullptr;
}

} // namespace nx::vms::testcamera
