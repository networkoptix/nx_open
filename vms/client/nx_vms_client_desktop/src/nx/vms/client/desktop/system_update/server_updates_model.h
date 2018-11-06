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

namespace nx::vms::client::desktop {

using StatusCode = nx::update::Status::Code;

// This structure keeps all the information necessary to display current state of the server.
struct UpdateItem
{
    enum Roles
    {
        UpdateItemRole = Qn::RoleCount,
    };

    QnMediaServerResourcePtr server;
    StatusCode state = StatusCode::offline;
    int progress = -1;
    QString statusMessage;

    // Flag for servers, that can be updated using legacy 3.2 system
    bool onlyLegacyUpdate = false;
    bool legacyUpdateUsed = false;
    bool offline = false;
    bool skipped = false;
    bool installed = false;
    bool changedProtocol = false;
    bool installing = false;
    bool storeUpdates = true;
    // Row in the table
    int row = -1;
};

using UpdateItemPtr = std::shared_ptr<UpdateItem>;

// Model class to represent update states of the server
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

    explicit ServerUpdatesModel(QObject* parent);

    QnServerUpdatesColors colors() const;
    void setColors(const QnServerUpdatesColors& colors);

    UpdateItemPtr findItemById(QnUuid id);
    UpdateItemPtr findItemByRow(int row) const;

    // Overrides status for chosen mediaservers.
    void setManualStatus(QSet<QnUuid> targets, nx::update::Status::Code status);

    // Overrides for QAbstractTableModel
    int columnCount(const QModelIndex& parent) const override;
    int rowCount(const QModelIndex& parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    // Need this to allow delegate to spawn editor
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setUpdateTarget(const nx::utils::SoftwareVersion& version);

    nx::utils::SoftwareVersion lowestInstalledVersion();

    // Clears internal state back to initial state
    void clearState();

    // Called by rest api handler
    void setUpdateStatus(const std::map<QnUuid, nx::update::Status>& statusAll);

    // Set resource pool to be used as a source.
    void setResourceFeed(QnResourcePool* pool);

    const QList<UpdateItemPtr>& getServerData() const;

private:
    void resetResourses(QnResourcePool* pool);
    void updateVersionColumn();
    void updateContentsIndex();
    void addItemForServer(QnMediaServerResourcePtr server);

private:
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_resourceChanged(const QnResourcePtr& resource);

    // Reads data from resource to UpdateItem
    void updateServerData(QnMediaServerResourcePtr server, UpdateItem& item);

private:
    QList<UpdateItemPtr> m_items;
    // The version we want to update to
    nx::utils::SoftwareVersion m_targetVersion;
    QnServerUpdatesColors m_versionColors;
};

} // namespace nx::vms::client::desktop

// I want to pass shared_ptr to QVariant
Q_DECLARE_METATYPE(std::shared_ptr<nx::vms::client::desktop::UpdateItem>);
