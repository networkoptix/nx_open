// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QAbstractProxyModel>
#include <QtCore/QSortFilterProxyModel>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <ui/models/systems_model.h>

class AbstractSystemsController;
class QnSystemsModelPrivate;

namespace nx::vms::client::core {

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
    Q_PROPERTY(nx::Uuid channelPartner
        READ channelPartner
        WRITE setChannelPartner
        NOTIFY channelPartnerChanged)
    Q_PROPERTY(bool channelPartnerLoading
        READ channelPartnerLoading
        NOTIFY channelPartnerLoadingChanged)

public:
    OrganizationsModel(QObject* parent = nullptr);
    virtual ~OrganizationsModel();

    Q_INVOKABLE QModelIndex indexFromId(nx::Uuid id) const;

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

    nx::Uuid channelPartner() const;
    void setChannelPartner(nx::Uuid channelPartner);

    bool channelPartnerLoading() const;

signals:
    void statusWatcherChanged();
    void systemsModelChanged();
    void sitesRootChanged();
    void channelPartnerChanged();
    void channelPartnerLoadingChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

// Hides 'SitesNode' root node and places its children last, optionally hiding SaaS systems.
class OrganizationsFilterModel: public QSortFilterProxyModel
{
    Q_OBJECT

    using base_type = QSortFilterProxyModel;

    Q_PROPERTY(bool hideOrgSystemsFromSites
        READ hideOrgSystemsFromSites
        WRITE setHideOrgSystemsFromSites
        NOTIFY hideOrgSystemsFromSitesChanged)

public:
    OrganizationsFilterModel(QObject* parent = nullptr);
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const;

    bool hideOrgSystemsFromSites() const { return m_hideOrgSystemsFromSites; }
    void setHideOrgSystemsFromSites(bool value);

signals:
    void hideOrgSystemsFromSitesChanged();

private:
    bool m_hideOrgSystemsFromSites = false;
};

} // namespace nx::vms::client::core
