// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QVector>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

Q_MOC_INCLUDE("core/resource/resource.h")
Q_MOC_INCLUDE("nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h")

namespace nx::vms::client::desktop {

class AccessSubjectEditingContext;

/**
 * An information about resource access details to be used in a QML subject editing UI.
 */
struct ResourceAccessInfo
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

    ProvidedVia providedVia{};
    QVector<QnUuid> providerUserGroups;
    QVector<QnResourcePtr> indirectProviders;
    int checkedChildCount = 0;
    int totalChildCount = 0;

    bool operator==(const ResourceAccessInfo& other) const;
    bool operator!=(const ResourceAccessInfo& other) const;
};

/**
 * An utility class to obtain information about resource access details in a QML subject editing UI.
 */
class ResourceAccessRightsModel: public QAbstractListModel
{
    Q_OBJECT

    /** A subject editing context. */
    Q_PROPERTY(nx::vms::client::desktop::AccessSubjectEditingContext* context READ context
        WRITE setContext NOTIFY contextChanged)

    /** A list of access rights of interest. */
    Q_PROPERTY(QVector<nx::vms::api::AccessRight> accessRightsList READ accessRightsList
        WRITE setAccessRightsList NOTIFY accessRightsListChanged)

    /** A resource of interest. */
    Q_PROPERTY(QnResource* resource READ resource WRITE setResource NOTIFY resourceChanged)

    /** Node type for current resource or top level tree item. */
    Q_PROPERTY(nx::vms::client::desktop::ResourceTree::NodeType nodeType READ nodeType
        WRITE setNodeType NOTIFY nodeTypeChanged)

    /** Id of a group of interest (all cameras, all web pages etc.) - deduced from node type. */
    Q_PROPERTY(QnUuid groupId READ groupId NOTIFY nodeTypeChanged)
    Q_PROPERTY(bool isResourceGroup READ isResourceGroup NOTIFY nodeTypeChanged)

    using base_type = QAbstractListModel;

public:
    enum Roles
    {
        ProviderRole = Qt::UserRole + 1,
        TotalChildCountRole,
        CheckedChildCountRole,
        AccessRightRole,
        EditableRole
    };

public:
    explicit ResourceAccessRightsModel(QObject* parent = nullptr);
    virtual ~ResourceAccessRightsModel() override;

    AccessSubjectEditingContext* context() const;
    void setContext(AccessSubjectEditingContext* value);

    QVector<nx::vms::api::AccessRight> accessRightsList() const;
    void setAccessRightsList(const QVector<nx::vms::api::AccessRight>& value);

    QnResource* resource() const;
    void setResource(QnResource* value);

    QnUuid groupId() const;
    bool isResourceGroup() const { return !groupId().isNull(); }

    ResourceTree::NodeType nodeType() const;
    void setNodeType(ResourceTree::NodeType value);

    QVector<ResourceAccessInfo> info() const;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QHash<int, QByteArray> roleNames() const override;

    /** @returns ProvidedVia::layout, ProvidedVia::videowall or ProvidedVia::unknown. */
    static Q_INVOKABLE ResourceAccessInfo::ProvidedVia providerType(QnResource* provider);

    Q_INVOKABLE void toggle(int index, bool withDependentAccessRights);

    static void registerQmlTypes();

signals:
    void contextChanged();
    void accessRightsListChanged();
    void resourceChanged();
    void nodeTypeChanged();
    void collapsedChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
