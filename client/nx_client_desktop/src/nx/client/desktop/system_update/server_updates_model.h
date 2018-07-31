#pragma once

#include <QtCore/QAbstractTableModel>
#include <memory> // for shared_ptr
#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <nx/vms/api/data/software_version.h>
#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>
#include <update/updates_common.h>

#include <nx/update/common_update_manager.h>

namespace nx {
namespace client {
namespace desktop {

using StatusCode = nx::UpdateStatus::Code;

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

    bool onlyLegacyUpdate = false;
    bool legacyUpdateUsed = false;
    bool offline = false;
    bool skipped = false;
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
        StatusColumn,
        ColumnCount
    };

    explicit ServerUpdatesModel(QObject *parent = 0);

    QnServerUpdatesColors colors() const;
    void setColors(const QnServerUpdatesColors &colors);

    UpdateItemPtr findItemById(QnUuid id);
    UpdateItemPtr findItemByRow(int row) const;
public:
    // Overrides for QAbstractTableModel
    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    // Need this to allow delegate to spawn editor
    Qt::ItemFlags flags(const QModelIndex &index) const override;

public:
    nx::utils::SoftwareVersion latestVersion() const;
    void setUpdateTarget(const nx::utils::SoftwareVersion &version,
        const QSet<nx::vms::api::SystemInformation>& selection);

    // Get current set of servers
    QSet<QnUuid> getAllServers() const;

    // Get servers with specified update status
    QSet<QnUuid> getServersInState(StatusCode status) const;

    // Get servers that are offline right now
    QSet<QnUuid> getOfflineServers() const;

    // Get servers that are incompatible with new update system
    QSet<QnUuid> getLegacyServers() const;

    // Called by rest api handler
    void setUpdateStatus(const std::map<QnUuid, nx::UpdateStatus>& statusAll);

    // Set resource pool to be used as a source.
    void setResourceFeed(QnResourcePool* pool);

    nx::utils::SoftwareVersion lowestInstalledVersion();

private:
    void resetResourses(QnResourcePool* pool);
    void updateVersionColumn();

    void updateContentsIndex();
    void addItemForServer(QnMediaServerResourcePtr server);

private:
    void at_resourceAdded(const QnResourcePtr &resource);
    void at_resourceRemoved(const QnResourcePtr &resource);
    void at_resourceChanged(const QnResourcePtr &resource);

    // Reads data from resource to UpdateItem
    void updateServerData(QnMediaServerResourcePtr server, UpdateItem& item);
private:
    QList<UpdateItemPtr> m_items;
    nx::utils::SoftwareVersion m_latestVersion;
    QSet<nx::vms::api::SystemInformation> m_updatePlatforms;

    QnServerUpdatesColors m_versionColors;
};

} // namespace desktop
} // namespace client
} // namespace nx

// I want to pass shared_ptr to QVariant
Q_DECLARE_METATYPE(std::shared_ptr<nx::client::desktop::UpdateItem>);
