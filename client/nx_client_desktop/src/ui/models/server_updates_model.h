#ifndef SERVER_UPDATES_MODEL_H
#define SERVER_UPDATES_MODEL_H

#include <QtCore/QAbstractTableModel>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <utils/common/system_information.h>
#include <utils/common/software_version.h>

#include <update/media_server_update_tool.h>

#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>

class QnServerUpdatesModel : public Customized<QAbstractTableModel>, public QnWorkbenchContextAware {
    Q_OBJECT

    Q_PROPERTY(QnServerUpdatesColors colors READ colors WRITE setColors)

    typedef Customized<QAbstractTableModel> base_type;
public:
    enum Columns {
        NameColumn,
        VersionColumn,
        ColumnCount
    };

    enum Roles {
        StageRole = Qn::RoleCount,
        ProgressRole
    };

    explicit QnServerUpdatesModel(QnMediaServerUpdateTool* tool, QObject *parent = 0);

    QnServerUpdatesColors colors() const;
    void setColors(const QnServerUpdatesColors &colors);

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(const QnMediaServerResourcePtr &server) const;
    QModelIndex index(const QnUuid &id) const;

    QnSoftwareVersion latestVersion() const;
    void setLatestVersion(const QnSoftwareVersion &version);

    QnCheckForUpdateResult checkResult() const;
    void setCheckResult(const QnCheckForUpdateResult &result);

    /** Lowest version from all online system servers. */
    QnSoftwareVersion lowestInstalledVersion() const;

signals:
    void lowestInstalledVersionChanged();

private:
    void resetResourses();
    void updateVersionColumn();
    void updateLowestInstalledVersion();

private slots:
    void at_resourceAdded(const QnResourcePtr &resource);
    void at_resourceRemoved(const QnResourcePtr &resource);
    void at_resourceChanged(const QnResourcePtr &resource);

private:
    class Item {
    public:
        Item(const QnMediaServerResourcePtr &server);

        QnMediaServerResourcePtr server() const;
        QnPeerUpdateStage stage() const;

        QVariant data(int column, int role) const;

    private:
        QnMediaServerResourcePtr m_server;
        QnPeerUpdateStage m_stage;
        int m_progress;

        friend class QnServerUpdatesModel;
    };

    QnMediaServerUpdateTool* m_updateTool;
    QList<Item*> m_items;

    /**
     * When a system is updated to an incompatible version (e.g. another protocol version) some
     * servers may finish updating before the current server is shut down. In this case they can
     * be detected (and then displayed by this model) as incompatible servers. This behavior
     * confuses users and should be avoided. This black-list avoids appearance of fake server
     * resources corresponding to servers which are online (and thus will be updated) at the moment
     * of update start.
     */
    QSet<QnUuid> m_incompatibleServersBlackList;

    QnSoftwareVersion m_latestVersion;
    QnSoftwareVersion m_lowestInstalledVersion;
    QnCheckForUpdateResult m_checkResult;
    QnServerUpdatesColors m_colors;
};

#endif // SERVER_UPDATES_MODEL_H
