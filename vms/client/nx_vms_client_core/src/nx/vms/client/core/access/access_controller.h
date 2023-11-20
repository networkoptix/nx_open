// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::vms::api { struct UserGroupData; }

namespace nx::vms::client::core {

//-------------------------------------------------------------------------------------------------
// AccessController

class NX_VMS_CLIENT_CORE_API AccessController:
    public QObject,
    private nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

protected:
    using GlobalPermissions = nx::vms::api::GlobalPermissions;
    using SystemContext = nx::vms::common::SystemContext;
    using UserGroupData = nx::vms::api::UserGroupData;

public:
    explicit AccessController(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~AccessController() override;

    using SystemContextAware::systemContext;

    /** Get/set current authority user. */
    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr& value);

    /** Get current permissions for given target. */
    Qn::Permissions permissions(const QnResourcePtr& targetResource) const;
    Qn::Permissions permissions(const UserGroupData& targetUserGroup) const;
    bool hasPermissions(const QnResourcePtr& targetResource, Qn::Permissions desired) const;
    bool hasPermissions(const UserGroupData& targetUserGroup, Qn::Permissions desired) const;
    bool hasPermissions(const QnUuid& subjectId, Qn::Permissions desired) const;
    bool hasAnyPermission(const QnResourcePtr& targetResource, Qn::Permissions desired) const;

    /** Get current user's global permissions. */
    GlobalPermissions globalPermissions() const;
    bool hasGlobalPermissions(GlobalPermissions desired) const;
    bool hasPowerUserPermissions() const;

    /**
     * Checks whether specifed access rights are currently relevant for any device to determine
     * what global operations are available (e.g. show Bookmarks panel or not, etc.)
     */
    virtual bool isDeviceAccessRelevant(nx::vms::api::AccessRights requiredAccessRights) const;

    /**
     * Create or return existing shared notifier of permission changes for specified resource.
     * While a notifier exists for a resource, permissions for that resource are cached.
     *
     * Notes:
     *     - you may not create a notifier for a resource not in a resource pool or from
     *       a different system context;
     *     - in such erroneous cases a valid pointer to a dummy notifier will be returned;
     *     - when notifier's watched resource is removed from the pool, the notifier is detached
     *       from the access controller and stops emitting signals;
     *     - notifier does NOT emit `permissionsChanged` when resource is removed from the pool,
     *       but AccessController does emit its collective `permissionsMaybeChanged` signal;
     *     - as notifiers are shared, before clearing shared pointers always disconnect notifiers
     *       from your receivers.
     */
    class Notifier;
    using NotifierPtr = std::shared_ptr<Notifier>;
    class NotifierHelper;
    NotifierPtr createNotifier(const QnResourcePtr& targetResource);

protected:
    virtual GlobalPermissions calculateGlobalPermissions() const;
    virtual Qn::Permissions calculatePermissions(const QnResourcePtr& targetResource) const;
    virtual Qn::Permissions calculatePermissions(const UserGroupData& targetUserGroup) const;

    void updatePermissions(const QnResourceList& targetResources);

signals:
    void userChanged();

    /**
     * Emitted if client current permissions could have been changed.
     * For precise per-resource permissions tracking use notifiers instead.
     * Note that this signal is also emitted when resources are added to or removed from the pool.
     * `resourcesHint` indicates affected resources if not empty; if empty, any resource can be
     * affected.
     */
    void permissionsMaybeChanged(const QnResourceList& resourcesHint, QPrivateSignal);

    void globalPermissionsChanged(GlobalPermissions current, GlobalPermissions old, QPrivateSignal);

    void administrativePermissionsChanged(GlobalPermissions current, GlobalPermissions old,
        QPrivateSignal);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

//-------------------------------------------------------------------------------------------------
// AccessController::Notifier

class NX_VMS_CLIENT_CORE_API AccessController::Notifier: public QObject
{
    Q_OBJECT

signals:
    void permissionsChanged(const QnResourcePtr& targetResource, Qn::Permissions current,
        Qn::Permissions old, AccessController::QPrivateSignal);
};

//-------------------------------------------------------------------------------------------------
// AccessController::NotifierHelper

class NX_VMS_CLIENT_CORE_API AccessController::NotifierHelper
{
public:
    NotifierHelper() = default;
    NotifierHelper(NotifierPtr newNotifier): notifier(newNotifier) {}

    void reset(NotifierPtr newNotifier = {})
    {
        connections = {};
        notifier = newNotifier;
    }

    template<typename Receiver, typename Handler>
    void callOnPermissionsChanged(Receiver receiver, Handler handler,
        Qt::ConnectionType connectionType = Qt::AutoConnection)
    {
        connections << QObject::connect(notifier.get(),
            &AccessController::Notifier::permissionsChanged,
            receiver,
            handler,
            connectionType);
    }

private:
    NotifierPtr notifier;
    nx::utils::ScopedConnections connections;
};

} // namespace nx::vms::client::core
