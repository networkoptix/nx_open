#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>

#include <common/common_module_aware.h>
#include <client_core/connection_context_aware.h>
#include <utils/common/connective.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

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
    Q_PROPERTY(QnUuid resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)

public:
    TwoWayAudioController(QObject* parent = nullptr);

    virtual ~TwoWayAudioController();

    static void registerQmlType();

    using OperationCallback = std::function<void (bool callback)>;
    bool start(OperationCallback&& callback);
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

    bool started() const;

    void setSourceId(const QString& sourceId);

    bool available() const;

    QnUuid resourceId() const;
    void setResourceId(const QnUuid& value);

signals:
    void startedChanged();
    void resourceIdChanged();
    void availabilityChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
