// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory> // for shared_ptr

#include <QtCore/QAbstractTableModel>
#include <QtCore/QSortFilterProxyModel>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/software_version.h>
#include <ui/workbench/workbench_context_aware.h>

#include "peer_state_tracker.h"

namespace nx::vms::client::desktop {

/**
 * Represents current update status of the servers. Used to display servers table widget.
 */
class ServerUpdatesModel: public QAbstractTableModel, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QAbstractTableModel base_type;

public:
    enum Columns
    {
        NameColumn,
        VersionColumn,
        ProgressColumn,
        StorageSettingsColumn,
        StatusMessageColumn,
        ColumnCount
    };

    enum Roles
    {
        UpdateItemRole = Qn::ItemDataRoleCount,
    };

    explicit ServerUpdatesModel(
        std::shared_ptr<PeerStateTracker> tracker,
        QObject* parent);

    // Overrides for QAbstractTableModel
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    // Need this to allow delegate to spawn editor
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setUpdateTarget(const nx::utils::SoftwareVersion& version);

    // Clears internal state back to initial state
    void clearState();
    void forceUpdateColumn(Columns column);

private:
    void atItemChanged(UpdateItemPtr item);
    void atItemAdded(UpdateItemPtr item);
    void atItemRemoved(UpdateItemPtr item);

    // Reads data from resource to UpdateItem
    void updateServerData(QnMediaServerResourcePtr server, UpdateItem& item);
    void updateClientData();

private:
    std::shared_ptr<PeerStateTracker> m_tracker;
    // The version we want to update to
    nx::utils::SoftwareVersion m_targetVersion;
};

class SortedPeerUpdatesModel: public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit SortedPeerUpdatesModel(QObject* parent = nullptr);

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

} // namespace nx::vms::client::desktop

// I want to pass shared_ptr to QVariant
Q_DECLARE_METATYPE(std::shared_ptr<nx::vms::client::desktop::UpdateItem>);
