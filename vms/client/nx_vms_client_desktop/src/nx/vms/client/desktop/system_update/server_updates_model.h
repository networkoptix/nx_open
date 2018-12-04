#pragma once

#include <QtCore/QAbstractTableModel>
#include <memory> // for shared_ptr

#include <client/client_color_types.h> // For QnServerUpdatesColors.

#include <core/resource/resource_fwd.h>

#include <nx/vms/api/data/software_version.h>
#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>
#include <update/updates_common.h>
#include <nx/update/common_update_manager.h>
#include "peer_state_tracker.h"

namespace nx::vms::client::desktop {

/**
 * Represents current update status of the servers. Used to display servers table widget.
 */
class ServerUpdatesModel : public Customized<QAbstractTableModel>, public QnWorkbenchContextAware
{
    Q_OBJECT

    Q_PROPERTY(QnServerUpdatesColors colors READ colors WRITE setColors)

    typedef Customized<QAbstractTableModel> base_type;

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

    explicit ServerUpdatesModel(
        std::shared_ptr<PeerStateTracker> tracker,
        QObject* parent);

    QnServerUpdatesColors colors() const;
    void setColors(const QnServerUpdatesColors& colors);
    void setServersInstalling(QSet<QnUuid> targets, bool installing = true);

    // Overrides for QAbstractTableModel
    int columnCount(const QModelIndex& parent) const override;
    int rowCount(const QModelIndex& parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    // Need this to allow delegate to spawn editor
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setUpdateTarget(const nx::utils::SoftwareVersion& version);

    // Clears internal state back to initial state
    void clearState();

private:
    void resetResourses(QnResourcePool* pool);
    void updateVersionColumn();
    //void updateContentsIndex();

    void addItemForServer(QnMediaServerResourcePtr server);
    void addItemForClient();

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
    QnServerUpdatesColors m_versionColors;
};

class SortedPeerUpdatesModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit SortedPeerUpdatesModel(QObject *parent = 0);
    void setShowClients(bool show);

protected:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    bool m_showClients = false;
};

} // namespace nx::vms::client::desktop

// I want to pass shared_ptr to QVariant
Q_DECLARE_METATYPE(std::shared_ptr<nx::vms::client::desktop::UpdateItem>);
