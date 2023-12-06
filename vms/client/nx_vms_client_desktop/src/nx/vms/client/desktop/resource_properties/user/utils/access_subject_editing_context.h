// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_details.h>
#include <core/resource_access/resource_access_map.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_model_adapter.h>

namespace nx::vms::common { class SystemContext; }
namespace nx::core::access { class SubjectHierarchy; }

namespace nx::vms::client::desktop::ResourceTree { enum class NodeType; }

class QnResourcePool;

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API ResourceAccessTarget
{
public:
    ResourceAccessTarget() = default;
    ResourceAccessTarget(const ResourceAccessTarget&) = default;
    ResourceAccessTarget(ResourceAccessTarget&&) = default;
    ResourceAccessTarget& operator=(const ResourceAccessTarget&) = default;
    ResourceAccessTarget& operator=(ResourceAccessTarget&&) = default;

    ResourceAccessTarget(const QnResourcePtr& resource);
    ResourceAccessTarget(nx::vms::api::SpecialResourceGroup group);
    ResourceAccessTarget(const QnUuid& specialResourceGroupId);
    ResourceAccessTarget(QnResourcePool* resourcePool, const QnUuid& resourceOrGroupId);

    QnUuid id() const { return m_id; }
    QnResourcePtr resource() const { return m_resource; }
    QnUuid groupId() const { return m_group ? m_id : QnUuid{}; }
    std::optional<nx::vms::api::SpecialResourceGroup> group() const { return m_group; }

    bool isValid() const { return m_resource || m_group; }

    QString toString() const;

private:
    bool isResource() const { return (bool) m_resource; }
    bool isGroup() const { return (bool) m_group; }

private:
    QnResourcePtr m_resource;
    std::optional<nx::vms::api::SpecialResourceGroup> m_group;
    QnUuid m_id;

    Q_GADGET
    Q_PROPERTY(QnUuid id READ id CONSTANT)
    Q_PROPERTY(bool isValid READ isValid CONSTANT)
    Q_PROPERTY(bool isResource READ isResource CONSTANT)
    Q_PROPERTY(bool isGroup READ isGroup CONSTANT)
};

struct NX_VMS_CLIENT_DESKTOP_API ResourceAccessTreeItem
{
    enum class Type
    {
        groupingNode,
        specialResourceGroup,
        resource
    };
    Q_ENUM(Type);

    Type type{};
    ResourceAccessTarget target;
    ResourceTree::NodeType nodeType;
    QnUuid outerSpecialResourceGroupId;
    nx::vms::api::AccessRights relevantAccessRights;

    Q_GADGET
};

class NX_VMS_CLIENT_DESKTOP_API AccessSubjectEditingContext: public QObject
{
    Q_OBJECT
    using base_type = QObject;
    using IsIndexAccepted = ResourceSelectionModelAdapter::IsIndexAccepted;

    Q_PROPERTY(IsIndexAccepted accessibleByPermissionsFilter
        READ accessibleByPermissionsFilter NOTIFY accessibleByPermissionsFilterChanged)

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

    /** Returns fully resolved access rights for a specified resource or resource group. */
    nx::vms::api::AccessRights accessRights(const ResourceAccessTarget& target) const;

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
     * Returns resource filter that accepts model indexes containing rows for which the current
     * subject has had any access rights since resetAccessibleByPermissionsFilter() was called.
     */
    IsIndexAccepted accessibleByPermissionsFilter() const;
    Q_INVOKABLE void resetAccessibleByPermissionsFilter();

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

    static Q_INVOKABLE nx::vms::api::AccessRights relevantAccessRights(
        const ResourceAccessTarget& target);

    Q_INVOKABLE nx::vms::api::AccessRights combinedRelevantAccessRights(
        const QModelIndexList& indexes) const;

    static void modifyAccessRightMap(nx::core::access::ResourceAccessMap& accessRightMap,
        const QnUuid& resourceOrGroupId,
        nx::vms::api::AccessRights modifiedRightsMask,
        bool value,
        bool withDependent = false,
        nx::vms::api::AccessRights relevantRightsMask = nx::vms::api::kFullAccessRights);

    Q_INVOKABLE Qt::CheckState combinedOwnCheckState(const QModelIndexList& indexes,
        nx::vms::api::AccessRight accessRight) const;

    Q_INVOKABLE void modifyAccessRights(const QModelIndexList& indexes,
        nx::vms::api::AccessRights modifiedRightsMask, bool value, bool withDependent);

    Q_INVOKABLE void modifyAccessRight(const QModelIndex& objectIndex,
        int /*AccessRight*/ accessRight, bool value, bool withDependentAccessRights);

    void modifyAccessRight(const QModelIndex& objectIndex, nx::vms::api::AccessRight accessRight,
        bool value, bool withDependentAccessRights);

    static nx::vms::api::AccessRights dependentAccessRights(nx::vms::api::AccessRight dependingOn);
    static nx::vms::api::AccessRights requiredAccessRights(nx::vms::api::AccessRight requiredFor);
    static Q_INVOKABLE bool isDependingOn(int /*AccessRight*/ what, nx::vms::api::AccessRights on);
    static Q_INVOKABLE bool isRequiredFor(int /*AccessRight*/ what, nx::vms::api::AccessRights for_);

    QnResourceList getGroupResources(const QnUuid& resourceGroupId) const;
    static QnResourceList getChildResources(const QModelIndex& parentTreeNodeIndex);

    static QnUuid specialResourceGroupFor(const QnResourcePtr& resource);
    static QnUuid specialResourceGroup(ResourceTree::NodeType nodeType);

    static ResourceAccessTreeItem resourceAccessTreeItemInfo(
        const QModelIndex& resourceTreeModelIndex);

signals:
    void subjectChanged();
    void hierarchyChanged();
    void currentSubjectRemoved();
    void resourceAccessChanged();
    void resourceGroupsChanged(const QSet<QnUuid>& resourceGroupIds);
    void accessibleByPermissionsFilterChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
