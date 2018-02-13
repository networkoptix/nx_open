#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>

#include <common/common_module_aware.h>
#include <client_core/connection_context_aware.h>
#include <utils/license_usage_helper.h>

class QnSingleCamLicenseStatusHelper;

namespace nx {
namespace client {
namespace core {

class TwoWayAudioController: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QnConnectionContextAware;

    Q_PROPERTY(bool started READ started NOTIFY startedChanged)
public:
    TwoWayAudioController(QObject* parent = nullptr);
    TwoWayAudioController(
        const QString& sourceId,
        const QUuid& cameraId,
        QObject* parent = nullptr);

    virtual ~TwoWayAudioController();

    bool started() const;

    bool start();
    void stop();

    Q_INVOKABLE bool setStreamData(
        const QString& sourceId,
        const QUuid& cameraId);

signals:
    void startedChanged();
private:
    bool isAllowed() const;

    void setStarted(bool value);

private:
    bool m_started = false;
    rest::Handle m_startHandle = 0;
    QString m_sourceId;
    QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<QnSingleCamLicenseStatusHelper> m_licenseHelper;
};

} // namespace core
} // namespace client
} // namespace nx
