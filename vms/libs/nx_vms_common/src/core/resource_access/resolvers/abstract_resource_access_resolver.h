// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_details.h>
#include <core/resource_access/resource_access_map.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <utils/common/counter_hash.h>

class QnResourcePool;

namespace nx::core::access {

//-------------------------------------------------------------------------------------------------
// AbstractResourceAccessResolver

/**
 * A base class of resolvers that calculate direct or indirect access of users and groups
 * to cameras, web pages, health monitors, layouts and videowalls.
 */
class NX_VMS_COMMON_API AbstractResourceAccessResolver: public QObject
{
    using base_type = QObject;

public:
    explicit AbstractResourceAccessResolver(QObject* parent = nullptr);
    virtual ~AbstractResourceAccessResolver() override;

    /**
     * Returns access rights of the specified subject to the specified resource instance.
     * Resolves possible "global" access to all media and/or all videowalls.
     * No access is returned for resources that aren't in the resource pool.
     */
    virtual AccessRights accessRights(const QnUuid& subjectId,
        const QnResourcePtr& resource) const;

    /**
     * Returns all ways in which the specified subject gains specified access right to
     * the specified resource, directly and indirectly.
     */
    virtual ResourceAccessDetails accessDetails(
        const QnUuid& subjectId,
        const QnResourcePtr& resource,
        AccessRight accessRight) const = 0;

    /**
     * Returns raw access rights of the specified subject, per resource id.
     * Actual resource types, state and availability is not taken into account.
     * This is a primary overridable that resolves indirect resource access in derived classes.
     */
    virtual ResourceAccessMap resourceAccessMap(const QnUuid& subjectId) const = 0;

    /** Returns subject global permissions. */
    virtual nx::vms::api::GlobalPermissions globalPermissions(const QnUuid& subjectId) const = 0;

    /** Returns whether the subject has admin access rights. */
    bool hasFullAccessRights(const QnUuid& subjectId) const;

    /** An interface to subscribe to access rights change signals. */
    class Notifier;
    Notifier* notifier() const;

protected:
    /** Notification sink to be called when access rights of particular subjects are changed. */
    void notifyAccessChanged(const QSet<QnUuid>& subjectIds);

    /** Notification sink to be called when access rights of all subjects are reset. */
    void notifyAccessReset();

    /** Log pretty formatting functions. */
    static QString toLogString(const QnUuid& resourceId, QnResourcePool* resourcePool);
    static QString affectedCacheToLogString(const QSet<QnUuid>& affectedSubjectIds);

private:
    const std::unique_ptr<Notifier> m_notifier;
};

//-------------------------------------------------------------------------------------------------
// AbstractResourceAccessResolver::Notifier

/**
 * An internal notification helper that allows to subscribe for notifications about
 * access right changes for particular specified subjects.
 */
class NX_VMS_COMMON_API AbstractResourceAccessResolver::Notifier: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Notifier(QObject* parent = nullptr);
    virtual ~Notifier() override;

    /**
     * Subscribe to notifications about specified subjects access rights changes.
     * Subscriptions are reference-counted, `subscribeSubjects` increases reference
     * counters associated with specified subject ids.
     */
    void subscribeSubjects(const QSet<QnUuid>& subjectIds);

    /**
     * Unsubscribe from notifications about specified subjects access rights changes.
     * Subscriptions are reference-counted, `releaseSubjects` decreases reference counters
     * associated with specified subject ids; those subjects whose counters reach zero
     * are stopped being watched.
     */
    void releaseSubjects(const QSet<QnUuid>& subjectIds);

    /** Returns which subjects access rights changes are watched and reported. */
    QSet<QnUuid> watchedSubjectIds() const;

signals:
    /** Access rights of watched subjects have been changed. */
    void resourceAccessChanged(const QSet<QnUuid>& subjectIds);

    /** All subjects access rights have been reset. */
    void resourceAccessReset();

    void subjectsSubscribed(const QSet<QnUuid>& subjectIds);
    void subjectsReleased(const QSet<QnUuid>& subjectIds);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::access
