// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_details.h>
#include <core/resource_access/resource_access_map.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/types/access_rights_types.h>

class QnResourcePool;

namespace nx::core::access {

class AbstractAccessRightsManager;
class AbstractGlobalPermissionsWatcher;
class SubjectHierarchy;

/**
 * A convenience wrapper class that encapsulates the stack of all resource access resolvers.
 * Its interface is similar to intrinsic AbstractResourceAccessResolver, but it isn't derived from
 * it.
 */
class NX_VMS_COMMON_API AccessRightsResolver: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum class Mode
    {
        normal, //< Normal resolver operation.
        editing //< Access subject editing operation.
    };

    explicit AccessRightsResolver(
        QnResourcePool* resourcePool,
        AbstractAccessRightsManager* accessRightsManager,
        AbstractGlobalPermissionsWatcher* globalPermissionsWatcher,
        SubjectHierarchy* subjectHierarchy,
        Mode mode = Mode::normal,
        QObject* parent = nullptr);

    virtual ~AccessRightsResolver() override;

    /**
     * Returns access rights of the specified subject to the specified resource instance.
     * Resolves indirect resource access and common access to all media and/or all videowalls.
     * No access is returned for resources that aren't in the resource pool.
     */
    nx::vms::api::AccessRights accessRights(const QnUuid& subjectId,
        const QnResourcePtr& resource) const;

    /**
     * Returns access rights of the specified subject to the specified resource group.
     * Currently only special resource groups (such as all devices or all videowalls) are supported.
     */
    nx::vms::api::AccessRights accessRights(const QnUuid& subjectId,
        const QnUuid& resourceGroupId) const;

    /** Returns subject global permissions. */
    nx::vms::api::GlobalPermissions globalPermissions(const QnUuid& subjectId) const;

    /** Returns subject's resolved access rights map. */
    ResourceAccessMap resourceAccessMap(const QnUuid& subjectId) const;

    /**
     * A combination of all access rights of the specified subject.
     * Currently this method is not usable because due to the server implementation when a resource
     * is deleted its id stays in all subject access right lists it was in, so this method will
     * return a combination of access rights towards both actual and no longer actual resources.
     */
    nx::vms::api::AccessRights availableAccessRights(const QnUuid& subjectId) const;

    /** Returns whether the subject has admin access rights. */
    bool hasFullAccessRights(const QnUuid& subjectId) const;

    /**
     * Returns all ways in which the specified subject gains specified access right to
     * the specified resource, directly and indirectly.
     */
    ResourceAccessDetails accessDetails(
        const QnUuid& subjectId,
        const QnResourcePtr& resource,
        nx::vms::api::AccessRight accessRight) const;

    /** A helper function to create an access rights change notifier connected to this resolver. */
    class Notifier;
    Notifier* createNotifier(QObject* parent = nullptr);

signals:
    /** Access rights of all subjects have been reset. */
    void resourceAccessReset(QPrivateSignal);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

/**
 * Access rights change notifier object.
 * Can be created either directly or via `AccessRightsResolver::createNotifier()`.
 */
class NX_VMS_COMMON_API AccessRightsResolver::Notifier: public QObject
{
    Q_OBJECT
    Q_PROPERTY(AccessRightsResolver* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QnUuid subjectId READ subjectId WRITE setSubjectId NOTIFY subjectIdChanged)

public:
    explicit Notifier(QObject* parent = nullptr);
    explicit Notifier(AccessRightsResolver* source, QObject* parent);
    virtual ~Notifier() override;

    /** Access rights resolver to subscribe with. */
    AccessRightsResolver* source() const;
    void setSource(AccessRightsResolver* value);

    /** A subject to watch access rights changes. */
    QnUuid subjectId() const;
    void setSubjectId(const QnUuid& value);

signals:
    /**
     * Sent when watched subject's access rights change.
     * It's not sent when all subjects access rights are reset.
     * `subjectId` argument is provided for additional convenience.
     */
    void resourceAccessChanged(const QnUuid& subjectId, QPrivateSignal);

    // Property notification signals.
    void sourceChanged();
    void subjectIdChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::access
