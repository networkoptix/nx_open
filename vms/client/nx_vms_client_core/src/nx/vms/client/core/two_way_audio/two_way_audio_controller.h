// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>

class SingleCamLicenseStatusHelper;

namespace nx::vms::client::core {

class TwoWayAudioAvailabilityWatcher;
class OrderedRequestsManager;

class NX_VMS_CLIENT_CORE_API TwoWayAudioController: public QObject, public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
    Q_PROPERTY(bool started READ started NOTIFY startedChanged)
    Q_PROPERTY(nx::Uuid resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)

public:
    TwoWayAudioController(SystemContext* systemContext, QObject* parent = nullptr);

    virtual ~TwoWayAudioController();

    static void registerQmlType();

    using OperationCallback = std::function<void (bool callback)>;
    bool start(OperationCallback&& callback);
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

    bool started() const;

    void setSourceId(const QString& sourceId);

    bool available() const;

    nx::Uuid resourceId() const;
    void setResourceId(const nx::Uuid& value);

signals:
    void startedChanged();
    void resourceIdChanged();
    void availabilityChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
