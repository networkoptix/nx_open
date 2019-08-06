#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>

#include <common/common_module_aware.h>
#include <client_core/connection_context_aware.h>
#include <utils/common/connective.h>

class QnSingleCamLicenseStatusHelper;

namespace nx::vms::client::core {

class TwoWayAudioAvailabilityWatcher;
class OrderedRequestsManager;

class TwoWayAudioController: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
    Q_PROPERTY(bool started READ started NOTIFY startedChanged)
    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)

public:
    TwoWayAudioController(QObject* parent = nullptr);
    TwoWayAudioController(
        const QString& sourceId,
        const QString& cameraId,
        QObject* parent = nullptr);

    virtual ~TwoWayAudioController();

    static void registerQmlType();

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

    bool started() const;

    void setSourceId(const QString& sourceId);

    bool available() const;

    QString resourceId() const;
    void setResourceId(const QString& value);

signals:
    void startedChanged();
    void resourceIdChanged();
    void availabilityChanged();

private:
    void setStarted(bool value);

private:
    const QScopedPointer<OrderedRequestsManager> m_requestsManager;
    bool m_started = false;
    bool m_available = false;
    QString m_sourceId;
    QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<TwoWayAudioAvailabilityWatcher> m_availabilityWatcher;
};

} // namespace nx::vms::client::core
