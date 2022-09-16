// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource_access/resource_access_details.h>
#include <core/resource_access/resource_access_map.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::common { class SystemContext; }
namespace nx::core::access { class SubjectHierarchy; }

namespace nx::vms::client::desktop {

class AccessSubjectEditingContext: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit AccessSubjectEditingContext(
        nx::vms::common::SystemContext* systemContext, QObject* parent = nullptr);

    virtual ~AccessSubjectEditingContext() override;

    nx::vms::common::SystemContext* systemContext() const;

    /** A current subject being edited. */
    QnUuid currentSubjectId() const;
    void setCurrentSubjectId(const QnUuid& subjectId);

    nx::core::access::ResourceAccessMap ownResourceAccessMap() const;

    /** Overrides current subject access rights. */
    void setOwnResourceAccessMap(const nx::core::access::ResourceAccessMap& resourceAccessMap);

    /** Reverts current subject access rights editing. */
    void resetOwnResourceAccessMap();

    /**
     * Returns all ways in which the specified subject gains specified access right to
     * the specified resource, directly and indirectly.
     */
    nx::core::access::ResourceAccessDetails accessDetails(
        const QnResourcePtr& resource,
        nx::vms::api::AccessRight accessRight) const;

    /** Edit current subject relations with other subjects. */
    void setRelations(const QSet<QnUuid>& parents, const QSet<QnUuid>& members);

    /** Revert current subject relations with other subjects to original values. */
    void resetRelations();

    /** Returns subject hierarchy being edited. */
    const nx::core::access::SubjectHierarchy* subjectHierarchy() const;

    /** Returns whether current subject access rights or inheritance were changed. */
    bool hasChanges() const;

    /** Reverts any changes. */
    void revert();

    Q_INVOKABLE bool hasOwnAccessRight(
        const QnUuid& resourceId,
        nx::vms::api::AccessRight accessRight) const;

    Q_INVOKABLE void setOwnAccessRight(
        const QnUuid& resourceId,
        nx::vms::api::AccessRight accessRight,
        bool value = true);

    nx::vms::api::GlobalPermissions globalPermissions() const;

    QSet<QnUuid> globalPermissionSource(nx::vms::api::GlobalPermission perm) const;

signals:
    void subjectChanged();
    void hierarchyChanged();
    void currentSubjectRemoved();
    void resourceAccessChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
