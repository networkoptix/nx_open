#pragma once

#include <QtCore/QUrl>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_state_manager.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/id.h>
#include <nx/vms/api/data/software_version.h>

#include <update/updates_common.h>
#include "client_updates2_manager.h"

#include "update_data.h"

struct QnLowFreeSpaceWarning;

namespace Ui {
    class MultiServerUpdatesWidget;
} // namespace Ui

class QnSortedServerUpdatesModel;

namespace nx {
namespace client {
namespace desktop {

class ServerUpdateTool;
class ServerUpdatesModel;
class UploadManager;

class DownloadTool;

struct UpdateItem;

// Widget deals with update for multiple servers
// Widget is spawned as a tab for System Administraton menu
class MultiServerUpdatesWidget:
    public QnAbstractPreferencesWidget,
    public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;
    using LocalStatusCode = nx::api::Updates2StatusData::StatusCode;
public:
    MultiServerUpdatesWidget(QWidget* parent = nullptr);
    ~MultiServerUpdatesWidget();

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

    virtual void applyChanges() override;
    virtual void discardChanges() override;

    // Updates UI state to match internal state
    virtual void loadDataToUi() override;

    virtual bool hasChanges() const override;

    virtual bool canApplyChanges() const override;
    virtual bool canDiscardChanges() const override;

    bool cancelUpdatesCheck();
    bool canCancelUpdate() const;
    bool isUpdating() const;

    void at_updateFinished(const QnUpdateResult& result);
    void at_selfUpdateFinished(const QnUpdateResult& result);
    // Notifications from UpdateTool
    // We move from notification model to poll model, so no need for this methods
    //void at_tool_stageChanged(QnFullUpdateStage stage);
    //void at_tool_stageProgressChanged(QnFullUpdateStage stage, int progress);
    void at_tool_lowFreeSpaceWarning(QnLowFreeSpaceWarning& lowFreeSpaceWarning);
    //void at_tool_updatesCheckCanceled();
    void at_modelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

protected:
    // Callback for timer events
    void at_stateCheckTimer();

    void at_clickUpdateServers();
    void at_clickCheckForUpdates();
    void at_startUpdateAction();
    bool at_cancelCurrentAction();

    void at_updateItemCommand(std::shared_ptr<UpdateItem> item);

    void setModeLocalFile();
    void setModeSpecificBuild();
    void setModeLatestAvailable();
    void hideStatusColumns(bool value);

    void downloadClientUpdates();

private:
    // UI Mode for picking the source of update.
    enum class UpdateSourceMode
    {
        // Latest version from the update server.
        LatestVersion,
        // Specific build from the update server.
        SpecificBuild,
        // Manual update using local file.
        LocalFile
    };

    enum class WidgetUpdateState
    {
        // We have no information about remote state right now.
        Initial,
        // We have obtained some state from the servers. We can do some actions now.
        Ready,
        // We have started legacy update process. Maybe we do not need this
        LegacyUpdating,
        // We have issued a command to remote servers to start downloading the updates.
        RemoteDownloading,
        // Download update package locally.
        LocalDownloading,
        // Pushing local update package to server(s).
        LocalPushing,
        // Some servers have downloaded update data and ready to install it.
        ReadyInstall,
        // Some servers are installing an update.
        Installing,
    };

    static QString toString(WidgetUpdateState state);
    static QString toString(QnFullUpdateStage stage);
    static QString toString(LocalStatusCode stage);

    void setUpdateSourceMode(UpdateSourceMode mode);

    void initDropdownActions();
    void initDownloadActions();

    void autoCheckForUpdates();
    void checkForUpdates(bool fromInternet);

    // UI synhronization. This functions are ment to be called from loadDataToUi.
    // Do not call them from anywhere else.
    void syncUpdateSource();
    void syncRemoteUpdateState();

    static bool restartClient(const nx::utils::SoftwareVersion& version);

    struct ProgressInfo
    {
        int current = 0;
        int max = 0;
    };

    ProgressInfo calculateActionProgress() const;

    bool processRemoteChanges(bool force = false);
    bool processLegacyChanges(bool force = false);

    // Advances UI FSM towards selected state
    void moveTowardsState(WidgetUpdateState state, QSet<QnUuid> targets);

private:
    QScopedPointer<Ui::MultiServerUpdatesWidget> ui;

    // UI control flags. We run loadDataToUI periodically and check for this flags.
    bool m_updateLocalStateChanged = true;
    bool m_updateRemoteStateChanged = true;
    bool m_latestVersionChanged = true;
    // Flag shows that we issued check for updates.
    bool m_checking = false;
    bool m_haveUpdate = false;

    UpdateSourceMode m_updateSourceMode = UpdateSourceMode::LatestVersion;
    // Task to check client updates from the internet.
    // We create it every time we issue another check for updates.
    // This class likes to self-destruct when request is finished.
    //QPointer<CheckForUpdatesTool> m_checkUpdatesTask;

    std::unique_ptr<ServerUpdateTool> m_updatesTool;
    std::unique_ptr<ServerUpdatesModel> m_updatesModel;
    std::unique_ptr<QnSortedServerUpdatesModel> m_sortedModel;
    // We use this one to download self-update locally.
    // There can be the case, when the server has no internet, but the client has.
    // So we could download update files for the server and push it manually.
    //std::unique_ptr<DownloadTool> m_downloader;

    // We cache previous state of update manger to notice changes in it.
    nx::api::Updates2StatusData::StatusCode m_lastStatus;

    std::unique_ptr<updates2::ClientUpdates2Manager> m_updateManager;

    std::unique_ptr<UploadManager> m_uploadManager;

    // Do we allow client to push updates?
    bool m_enableClientUpdates;

    WidgetUpdateState m_updateStateCurrent = WidgetUpdateState::Initial;
    WidgetUpdateState m_updateStateTarget = WidgetUpdateState::Initial;

    // We have issued an update for this version.
    // It can differ from m_localUpdateInfo.latestVersion.
    nx::utils::SoftwareVersion m_targetVersion;
    // Was ist das?
    QString m_targetChangeset;
    QString m_localFileName;

    struct LocalUpdateInfo
    {
        nx::utils::SoftwareVersion latestVersion;
        bool clientRequiresInstaller;
        UpdatePackageInfo clientUpdateFile;
        QHash<nx::utils::SoftwareVersion, UpdatePackageInfo> updateFiles;
    }m_localUpdateInfo;

    nx::utils::Url m_releaseNotesUrl;

    // Watchdog timer for the case when update has taken too long.
    std::unique_ptr<QTimer> m_longUpdateWarningTimer = nullptr;

    // Timer for updating interal state and synching it with UI.
    std::unique_ptr<QTimer> m_stateCheckTimer = nullptr;

    qint64 m_lastAutoUpdateCheck = 0;

    // URL of update path.
    QUrl m_updateSourcePath;

    // Set of servers that are currently active.
    QSet<QnUuid> m_serversActive;
    // Set of servers that are used for the current task.
    QSet<QnUuid> m_serversIssued;
    // A set of servers that have completed current task.
    QSet<QnUuid> m_serversComplete;
    // A set of servers that have failed current task.
    QSet<QnUuid> m_serversFailed;
    // A set of servers which task was canceled.
    QSet<QnUuid> m_serversCanceled;
};

} // namespace desktop
} // namespace client
} // namespace nx
