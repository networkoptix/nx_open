// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {

/**
 * A QML helper that allows to check current user's essential permissions towards a specified
 * resource, and signals when those permissions change.
 */
class AccessHelper:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    Q_PROPERTY(QnUuid resourceId READ resourceId WRITE setResourceId NOTIFY resourceChanged)
    Q_PROPERTY(QnResource* resource READ rawResource WRITE setRawResource NOTIFY resourceChanged)
    Q_PROPERTY(bool canViewContent READ canViewContent NOTIFY permissionsChanged)
    Q_PROPERTY(bool canViewLive READ canViewLive NOTIFY permissionsChanged)
    Q_PROPERTY(bool canViewArchive READ canViewArchive NOTIFY permissionsChanged)
    Q_PROPERTY(bool canViewBookmarks READ canViewBookmarks NOTIFY permissionsChanged)
    Q_PROPERTY(bool canEditBookmarks READ canEditBookmarks NOTIFY permissionsChanged)
    Q_PROPERTY(bool canExportArchive READ canExportArchive NOTIFY permissionsChanged)
    Q_PROPERTY(bool canUsePtz READ canUsePtz NOTIFY permissionsChanged)
    Q_PROPERTY(bool canUseTwoWayAudio READ canUseTwoWayAudio NOTIFY permissionsChanged)
    Q_PROPERTY(bool canUseSoftTriggers READ canUseSoftTriggers NOTIFY permissionsChanged)
    Q_PROPERTY(bool canUseDeviceIO READ canUseDeviceIO NOTIFY permissionsChanged)

public:
    explicit AccessHelper(SystemContext* context, QObject* parent = nullptr);
    explicit AccessHelper(); //< Constructor for QML.
    virtual ~AccessHelper() override;

    QnUuid resourceId() const;
    void setResourceId(const QnUuid& value);

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr& value);

    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

    bool canViewContent() const;
    bool canViewLive() const;
    bool canViewArchive() const;
    bool canViewBookmarks() const;
    bool canEditBookmarks() const;
    bool canExportArchive() const;
    bool canUsePtz() const;
    bool canUseTwoWayAudio() const;
    bool canUseSoftTriggers() const;
    bool canUseDeviceIO() const;

    static void registerQmlType();

signals:
    void resourceChanged();
    void permissionsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
