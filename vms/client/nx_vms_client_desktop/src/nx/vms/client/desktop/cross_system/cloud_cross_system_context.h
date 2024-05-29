// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/core/network/remote_connection_fwd.h>
#include <nx/vms/client/core/resource/resource_fwd.h>

class QnBaseSystemDescription;
using QnSystemDescriptionPtr = QSharedPointer<QnBaseSystemDescription>;

namespace nx::vms::client::desktop {

class SystemContext;

class CloudCrossSystemContext: public QObject
{
    Q_OBJECT

public:
    using base_type = QObject;

public:
    CloudCrossSystemContext(QnSystemDescriptionPtr systemDescription, QObject* parent = nullptr);
    virtual ~CloudCrossSystemContext() override;

    enum class Status
    {
        uninitialized,
        connecting,
        connectionFailure,
        unsupportedPermanently,
        unsupportedTemporary,
        connected,
    };
    Q_ENUM(Status);

    Status status() const;

    /** Returns whether the system the context belongs is connected and online. */
    bool isSystemReadyToUse() const;

    QString systemId() const;

    QnBaseSystemDescription* systemDescription() const;

    SystemContext* systemContext() const;

    QnVirtualCameraResourceList cameras() const;

    /**
     * Debug log representation. Used by toString(const T*).
     */
    QString idForToStringFromPtr() const;

    /**
     * Debug log representation.
     */
    QString toString() const;

    /**
     * Try to establish connection with user interaction allowed.
     * Return true if new connection is started.
     */
    bool initializeConnectionWithUserInteraction();

    QnVirtualCameraResourcePtr createThumbCameraResource(const nx::Uuid& id, const QString& name);

    bool needsCloudAuthorization();
    void cloudAuthorize();

signals:
    void statusChanged(Status oldStatus);
    void camerasAdded(const QnVirtualCameraResourceList& cameras);
    void camerasRemoved(const QnVirtualCameraResourceList& cameras);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

QString toString(CloudCrossSystemContext::Status status);

using CloudCrossSystemContextPtr = std::shared_ptr<CloudCrossSystemContext>;

} // namespace nx::vms::client::desktop
