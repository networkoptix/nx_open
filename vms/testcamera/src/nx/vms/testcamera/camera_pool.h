#pragma once

#include <memory>
#include <map>
#include <optional>

#include <QtCore/QMutex>
#include <QtCore/QStringList>

#include <network/tcp_listener.h>

#include "camera.h"
#include "network_options.h"

class QnCommonModule;

namespace nx::vms::testcamera {

class FileCache;
class FrameLogger; //< private
class CameraDiscoveryListener; //< private
class CameraDiscoveryResponse;

/**
 * Runs discovery service which processes camera discovery messages, and creates the cameras in
 * response to streaming requests from Servers.
 */
class CameraPool: public QnTcpListener
{
public:
    CameraPool(
        const FileCache* fileCache,
        NetworkOptions networkOptions,
        QnCommonModule* commonModule,
        bool noSecondaryStream,
        std::optional<int> fpsPrimary,
        std::optional<int> fpsSecondary,
        const std::optional<QString>& frameLogFilename);

    virtual ~CameraPool();

    int cameraCount() const { return (int) m_cameraByMacAddress.size(); }

    bool addCameraSet(
        bool cameraForEachFile,
        const CameraOptions& cameraOptions,
        int count,
        const QStringList& primaryFileNames,
        const QStringList& secondaryFileNames);

    bool startDiscovery();

    /** @return Null if not found. */
    Camera* findCamera(const nx::utils::MacAddress& macAddress) const;

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket) override;

private:
    QByteArray obtainDiscoveryResponseMessage() const;

    void reportAddingCameras(
        bool cameraForEachFile,
        const CameraOptions& cameraOptions,
        int count,
        const QStringList& primaryFileNames,
        const QStringList& secondaryFileNames);

    bool addCamera(
        const CameraOptions& cameraOptions,
        QStringList primaryFileNames,
        QStringList secondaryFileNames);

private:
    const FileCache* const m_fileCache;
    const NetworkOptions m_networkOptions;
    const std::unique_ptr<Logger> m_logger;
    const std::unique_ptr<FrameLogger> m_frameLogger;
    std::map<nx::utils::MacAddress, std::unique_ptr<Camera>> m_cameraByMacAddress;
    std::map<nx::utils::MacAddress, std::shared_ptr<CameraDiscoveryResponse>>
        m_cameraDiscoveryResponseByMacAddress;
    mutable QMutex m_mutex;
    std::unique_ptr<CameraDiscoveryListener> m_discoveryListener;
    const bool m_noSecondaryStream;
    const std::optional<int> m_fpsPrimary;
    const std::optional<int> m_fpsSecondary;
};

} // namespace nx::vms::testcamera
