#pragma once

#include <memory>
#include <map>

#include <QtCore/QMutex>
#include <QtCore/QStringList>

#include <network/tcp_listener.h>

#include "camera.h"

class QnCommonModule;

namespace nx::vms::testcamera {

class FileCache;
class FrameLogger; //< private
class CameraDiscoveryListener; //< private

/**
 * Runs discovery service which processes camera discovery messages, and creates the cameras in
 * response to streaming requests from Servers.
 */
class CameraPool: public QnTcpListener
{
    using base_type = QnTcpListener;

public:
    /**
     * @param localInterfacesToListen If empty, all local interfaces are being listened to.
     */
    CameraPool(
        QStringList localInterfacesToListen,
        QnCommonModule* commonModule,
        bool noSecondaryStream,
        int fpsPrimary,
        int fpsSecondary);

    virtual ~CameraPool();

    int cameraCount() const { return (int) m_cameraByMac.size(); }

    void addCameras(
        const FileCache* fileCache,
        bool cameraForEachFile,
        const CameraOptions& testCameraOptions,
        int count,
        const QStringList& primaryFileNames,
        const QStringList& secondaryFileNames);

    bool startDiscovery();

    /** @return Null if not found. */
    Camera* findCamera(const QString& mac) const;

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket) override;

private:
    void reportAddingCameras(
        bool cameraForEachFile,
        const CameraOptions& testCameraOptions,
        int count,
        const QStringList& primaryFileNames,
        const QStringList& secondaryFileNames);

private:
    const QStringList m_localInterfacesToListen;
    const std::unique_ptr<Logger> m_logger;
    const std::unique_ptr<FrameLogger> m_frameLogger;
    std::map<QString, std::unique_ptr<Camera>> m_cameraByMac;
    mutable QMutex m_mutex;
    std::unique_ptr<CameraDiscoveryListener> m_discoveryListener;
    const bool m_noSecondaryStream;
    const int m_fpsPrimary;
    const int m_fpsSecondary;
};

} // namespace nx::vms::testcamera
