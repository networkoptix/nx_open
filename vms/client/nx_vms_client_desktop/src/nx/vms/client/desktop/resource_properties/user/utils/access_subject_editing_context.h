// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
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

    Q_PROPERTY(nx::vms::common::ResourceFilter accessibleResourcesFilter
        READ accessibleResourcesFilter NOTIFY accessibleResourcesFilterChanged)

public:
    explicit AccessSubjectEditingContext(
        nx::vms::common::SystemContext* systemContext, QObject* parent = nullptr);

    virtual ~AccessSubjectEditingContext() override;

    nx::vms::common::SystemContext* systemContext() const;

    /** A current subject being edited. */
    QnUuid currentSubjectId() const;

    enum class SubjectType
    {
        user,
        group
    };
    Q_ENUM(SubjectType);

    SubjectType currentSubjectType() const;
    void setCurrentSubject(const QnUuid& subjectId, SubjectType type);

    nx::core::access::ResourceAccessMap ownResourceAccessMap() const;

    bool hasOwnAccessRight(
        const QnUuid& resourceOrGroupId, nx::vms::api::AccessRight accessRight) const;

    /** Overrides current subject access rights. */
    void setOwnResourceAccessMap(const nx::core::access::ResourceAccessMap& resourceAccessMap);

    /** Reverts current subject access rights editing. */
    void resetOwnResourceAccessMap();

    /** Returns full resolved access rights for a specified resource. */
    Q_INVOKABLE nx::vms::api::AccessRights accessRights(const QnResourcePtr& resource) const;

    /**
     * Returns all ways in which the specified subject gains specified access right to
     * the specified resource, directly and indirectly.
     */
    nx::core::access::ResourceAccessDetails accessDetails(
        const QnResourcePtr& resource,
        nx::vms::api::AccessRight accessRight) const;

    /**
     * Returns direct parent user groups from which a specified access right to a specified
     * resource group is inherited.
     */
    QList<QnUuid> resourceGroupAccessProviders(
        const QnUuid& resourceGroupId,
        nx::vms::api::AccessRight accessRight) const;

    /**
     * Returns resource filter that accepts resources for which the current subject has had
     * any access rights since resetAccessibleResourcesFilter() was called.
     */
    nx::vms::common::ResourceFilter accessibleResourcesFilter() const;
    Q_INVOKABLE void resetAccessibleResourcesFilter();

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

    nx::vms::api::GlobalPermissions globalPermissions() const;
    nx::vms::api::AccessRights availableAccessRights() const;

    QSet<QnUuid> globalPermissionSource(nx::vms::api::GlobalPermission perm) const;

    static QnUuid specialResourceGroupFor(const QnResourcePtr& resource);

    static nx::vms::api::AccessRights relevantAccessRights(
        nx::vms::api::SpecialResourceGroup group);

    static nx::vms::api::AccessRights relevantAccessRights(const QnResourcePtr& resource);

    static void modifyAccessRightMap(nx::core::access::ResourceAccessMap& accessRightMap,
        const QnUuid& resourceOrGroupId,
        nx::vms::api::AccessRights modifiedRightsMask,
        bool value,
        bool withDependent,
        nx::vms::api::AccessRights relevantRightsMask);

    Q_INVOKABLE void modifyAccessRights(const QList<QnResource*>& resources,
        nx::vms::api::AccessRights modifiedRightsMask, bool value, bool withDependent);

    static nx::vms::api::AccessRights dependentAccessRights(nx::vms::api::AccessRight dependingOn);
    static nx::vms::api::AccessRights requiredAccessRights(nx::vms::api::AccessRight requiredFor);
    static Q_INVOKABLE bool isDependingOn(int /*AccessRight*/ what, nx::vms::api::AccessRights on);
    static Q_INVOKABLE bool isRequiredFor(int /*AccessRight*/ what, nx::vms::api::AccessRights for_);

signals:
    void subjectChanged();
    void hierarchyChanged();
    void currentSubjectRemoved();
    void resourceAccessChanged();
    void resourceGroupsChanged(const QSet<QnUuid>& resourceGroupIds);
    void accessibleResourcesFilterChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
