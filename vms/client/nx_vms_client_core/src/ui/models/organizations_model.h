// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QAbstractProxyModel>
#include <QtCore/QSortFilterProxyModel>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/utils/persistent_index_watcher.h>
#include <ui/models/systems_model.h>

class AbstractSystemsController;
class QnSystemsModelPrivate;

namespace nx::vms::client::core {

struct ChannelPartnerList;
struct OrganizationList;
struct GroupStructure;
struct SystemInOrganization;

/**
 * This model unifies organizations for a single channel partner (with systems and folder
 * structure) and non-SaaS systems in a tree, making its content viewable via a QML tree and
 * searchable through LinearizationListModel.
 * This model doesn't properly track row changes in persistent indexes when source model's layout
 * is changed or rows are moved.
 */
class NX_VMS_CLIENT_CORE_API OrganizationsModel: public QAbstractItemModel
{
    Q_OBJECT

    using base_type = QAbstractItemModel;

public:
    enum RoleId
    {
        FirstRole = QnSystemsModel::RolesCount,

        IdRole,
        TypeRole,
        SytemCountRole,
        SectionRole,
        IsLoadingRole,
        IsFromSitesRole,

        RolesCount
    };

    enum NodeType
    {
        None,
        ChannelPartner,
        Organization,
        Folder,
        System,
        SitesNode,
    };
    Q_ENUM(NodeType)

    Q_PROPERTY(CloudStatusWatcher* statusWatcher
        READ statusWatcher
        WRITE setStatusWatcher
        NOTIFY statusWatcherChanged);

    Q_PROPERTY(QAbstractProxyModel* systemsModel
        READ systemsModel
        WRITE setSystemsModel
        NOTIFY systemsModelChanged)
    Q_PROPERTY(QModelIndex sitesRoot
        READ sitesRoot
        NOTIFY sitesRootChanged)

    Q_PROPERTY(bool topLevelLoading
        READ topLevelLoading
        NOTIFY topLevelLoadingChanged)

    Q_PROPERTY(bool hasChannelPartners
        READ hasChannelPartners
        NOTIFY hasChannelPartnersChanged)

    Q_PROPERTY(bool hasOrganizations
        READ hasOrganizations
        NOTIFY hasOrganizationsChanged)

public:
    OrganizationsModel(QObject* parent = nullptr);
    virtual ~OrganizationsModel();

    Q_INVOKABLE QModelIndex indexFromNodeId(nx::Uuid id) const;
    Q_INVOKABLE QModelIndex indexFromSystemId(const QString& id) const;

public:
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;

    bool hasChildren(const QModelIndex &parent = QModelIndex()) const;

    CloudStatusWatcher* statusWatcher() const;
    void setStatusWatcher(CloudStatusWatcher* statusWatcher);

    QAbstractProxyModel* systemsModel() const;
    void setSystemsModel(QAbstractProxyModel* systemsModel);

    QModelIndex sitesRoot() const;

    bool topLevelLoading() const;
    bool hasChannelPartners() const;
    bool hasOrganizations() const;

    void setChannelPartners(const ChannelPartnerList& data);
    void setOrganizations(const OrganizationList& data, nx::Uuid parentId);
    void setOrgStructure(nx::Uuid orgId, const std::vector<GroupStructure>& data);
    void setOrgSystems(nx::Uuid orgId, const std::vector<SystemInOrganization>& data);

signals:
    void statusWatcherChanged();
    void systemsModelChanged();
    void sitesRootChanged();
    void topLevelLoadingChanged();
    void hasChannelPartnersChanged();
    void hasOrganizationsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

// Hides 'SitesNode' root node and places its children last, optionally hiding SaaS systems.
class NX_VMS_CLIENT_CORE_API OrganizationsFilterModel: public QSortFilterProxyModel
{
    Q_OBJECT

    using base_type = QSortFilterProxyModel;

    Q_PROPERTY(bool hideOrgSystemsFromSites
        READ hideOrgSystemsFromSites
        WRITE setHideOrgSystemsFromSites
        NOTIFY hideOrgSystemsFromSitesChanged)

    Q_PROPERTY(QList<OrganizationsModel::NodeType> showOnly
        READ showOnly
        WRITE setShowOnly
        NOTIFY showOnlyChanged)

    Q_PROPERTY(QModelIndex currentRoot
        READ currentRoot
        WRITE setCurrentRoot
        NOTIFY currentRootChanged);

public:
    OrganizationsFilterModel(QObject* parent = nullptr);
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const;

    bool hideOrgSystemsFromSites() const { return m_hideOrgSystemsFromSites; }
    void setHideOrgSystemsFromSites(bool value);

    void setShowOnly(const QList<OrganizationsModel::NodeType>& types);
    QList<OrganizationsModel::NodeType> showOnly() const { return m_showOnly.values(); }

    void setCurrentRoot(const QModelIndex& index);
    QModelIndex currentRoot() const;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

signals:
    void hideOrgSystemsFromSitesChanged();
    void showOnlyChanged();
    void currentRootChanged();

private:
    bool isInCurrentRoot(const QModelIndex& index) const;

private:
    bool m_hideOrgSystemsFromSites = false;
    QSet<OrganizationsModel::NodeType> m_showOnly;
    QPersistentModelIndex m_currentRoot;
    std::unique_ptr<PersistentIndexWatcher> m_currentRootWatcher;
};

} // namespace nx::vms::client::core
