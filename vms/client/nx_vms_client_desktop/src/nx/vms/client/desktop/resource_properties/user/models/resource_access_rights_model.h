// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QVector>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::desktop {

class AccessSubjectEditingContext;

/**
 * An information about resource access details to be used in a QML subject editing UI.
 */
struct NX_VMS_CLIENT_DESKTOP_API ResourceAccessInfo
{
    Q_GADGET

public:
    enum class ProvidedVia
    {
        none,
        own,
        layout,
        videowall,
        parentUserGroup,
        ownResourceGroup,
        unknown
    };
    Q_ENUM(ProvidedVia);

    ProvidedVia providedVia{ProvidedVia::none};
    ProvidedVia inheritedFrom{ProvidedVia::none};
    QVector<nx::Uuid> providerUserGroups;
    QVector<QnResourcePtr> indirectProviders;
    nx::Uuid parentResourceGroupId; //< Filled only if access is granted through it.
    int checkedChildCount = 0;
    int checkedAndInheritedChildCount = 0;
    int totalChildCount = 0;

    bool operator==(const ResourceAccessInfo& other) const;
    bool operator!=(const ResourceAccessInfo& other) const;
};

/**
 * An utility class to obtain information about resource access details in a QML subject editing UI.
 */
class NX_VMS_CLIENT_DESKTOP_API ResourceAccessRightsModel: public QAbstractListModel
{
    Q_OBJECT

    /** A subject editing context. */
    Q_PROPERTY(nx::vms::client::desktop::AccessSubjectEditingContext* context READ context
        WRITE setContext NOTIFY contextChanged)

    /** A list of access rights of interest. */
    Q_PROPERTY(QVector<nx::vms::api::AccessRight> accessRightsList READ accessRightsList
        WRITE setAccessRightsList NOTIFY accessRightsListChanged)

    Q_PROPERTY(QModelIndex resourceTreeIndex READ resourceTreeIndex WRITE setResourceTreeIndex
        NOTIFY resourceTreeIndexChanged)

    Q_PROPERTY(nx::vms::client::desktop::ResourceAccessTreeItem::Type rowType READ rowType
        NOTIFY resourceTreeIndexChanged)

    Q_PROPERTY(nx::vms::api::AccessRights relevantAccessRights READ relevantAccessRights
        NOTIFY resourceTreeIndexChanged)

    // Access rights granted to this target, or combined access rights granted to its children.
    Q_PROPERTY(nx::vms::api::AccessRights grantedAccessRights READ grantedAccessRights
        NOTIFY grantedAccessRightsChanged)

    // Do not display inheritance from these providers.
    Q_PROPERTY(QVariant /*QnResourceList or undefined*/ ignoredProviders
        READ ignoredProviders WRITE setIgnoredProviders NOTIFY ignoredProvidersChanged)

    using base_type = QAbstractListModel;

public:
    enum Roles
    {
        ProviderRole = Qt::UserRole + 1,
        InheritedFromRole,
        TotalChildCountRole,
        CheckedChildCountRole,
        CheckedAndInheritedChildCountRole,
        AccessRightRole,
        InheritanceInfoTextRole,
        EditableRole,
        DependentAccessRightsRole, //< Not intersected with relevantAccessRights yet.
        RequiredAccessRightsRole //< Not intersected with relevantAccessRights yet.
    };

public:
    explicit ResourceAccessRightsModel(QObject* parent = nullptr);
    virtual ~ResourceAccessRightsModel() override;

    AccessSubjectEditingContext* context() const;
    void setContext(AccessSubjectEditingContext* value);

    QVector<nx::vms::api::AccessRight> accessRightsList() const;
    void setAccessRightsList(const QVector<nx::vms::api::AccessRight>& value);

    QModelIndex resourceTreeIndex() const;
    void setResourceTreeIndex(const QModelIndex& value);

    QVariant ignoredProviders() const;
    void setIgnoredProviders(const QVariant& /*QnResourceList or undefined*/ value);

    ResourceAccessTreeItem::Type rowType() const;
    nx::vms::api::AccessRights relevantAccessRights() const;

    nx::vms::api::AccessRights grantedAccessRights() const;

    QVector<ResourceAccessInfo> info() const;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QHash<int, QByteArray> roleNames() const override;

    /** @returns ProvidedVia::layout, ProvidedVia::videowall or ProvidedVia::unknown. */
    static Q_INVOKABLE ResourceAccessInfo::ProvidedVia providerType(QnResource* provider);

    static void registerQmlTypes();

signals:
    void contextChanged();
    void accessRightsListChanged();
    void resourceTreeIndexChanged();
    void ignoredProvidersChanged();
    void grantedAccessRightsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
