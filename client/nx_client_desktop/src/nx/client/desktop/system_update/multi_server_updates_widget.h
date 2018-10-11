#pragma once

#include <memory>
#include <future>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_state_manager.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/id.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/update/common_update_manager.h>
#include <update/updates_common.h>

#include "server_update_tool.h"

struct QnLowFreeSpaceWarning;

namespace Ui { class MultiServerUpdatesWidget; }

class QnSortedServerUpdatesModel;

namespace nx {
namespace client {
namespace desktop {

class ServerUpdatesModel;
class ServerStatusItemDelegate;

struct UpdateItem;

// Widget deals with update for multiple servers.
// Widget is spawned as a tab for System Administraton menu.
class MultiServerUpdatesWidget:
    public QnAbstractPreferencesWidget,
    public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;
    using LocalStatusCode = nx::update::Status::Code;

public:
    MultiServerUpdatesWidget(QWidget* parent = nullptr);
    ~MultiServerUpdatesWidget();

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

    virtual void applyChanges() override;
    virtual void discardChanges() override;

    // Updates UI state to match internal state.
    virtual void loadDataToUi() override;

    virtual bool hasChanges() const override;

    virtual bool canApplyChanges() const override;
    virtual bool canDiscardChanges() const override;

    bool cancelUpdatesCheck();
    bool isChecking() const;

    //void at_updateFinished(const QnUpdateResult& result);
    //void at_selfUpdateFinished(const QnUpdateResult& result);

    void at_clientDownloadFinished();

    void at_modelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

protected:
    // Callback for timer events
    void at_updateCurrentState();

    void at_startUpdateAction();
    bool at_cancelCurrentAction();
    void hideStatusColumns(bool value);

    void clearUpdateInfo();
    void pickLocalFile();
    void pickSpecificBuild();

private:
    using UpdateCheckMode = ServerUpdateTool::UpdateCheckMode;

    enum class WidgetUpdateState
    {
        // We have no information about remote state right now.
        initial,
        // We have obtained some state from the servers. We can do some actions now.
        // Next action depends on m_updateSourceMode, and whether the update
        // is available for picked update source.
        ready,
        // We have issued a command to remote servers to start downloading the updates.
        downloading,
        // Pushing local update package to server(s).
        pushing,
        // Some servers have downloaded update data and ready to install it.
        readyInstall,
        // Some servers are installing an update.
        installing,
    };

    static QString toString(UpdateCheckMode mode);
    static QString toString(WidgetUpdateState state);
    static QString toString(LocalStatusCode stage);
    static QString toString(ServerUpdateTool::OfflineUpdateState state);

    void setUpdateSourceMode(UpdateCheckMode mode);

    void initDropdownActions();
    void initDownloadActions();

    void setAutoUpdateCheckMode(bool mode);
    void autoCheckForUpdates();
    void checkForInternetUpdates();

    // UI synhronization. This functions are ment to be called from loadDataToUi.
    // Do not call them from anywhere else.
    void syncUpdateSource();
    void syncRemoteUpdateState();

    static bool restartClient(const nx::utils::SoftwareVersion& version);

    ServerUpdateTool::ProgressInfo calculateActionProgress() const;

    bool processRemoteChanges(bool force = false);
    // Part of processRemoteChanges FSM processor.
    void processRemoteInitialState();
    void processRemoteDownloading(const ServerUpdateTool::RemoteStatus& remoteStatus);
    void processRemoteInstalling(const ServerUpdateTool::RemoteStatus& remoteStatus);

    bool processUploaderChanges(bool force = false);

    void closePanelNotifications();

    // Advances UI FSM towards selected state.
    void setTargetState(WidgetUpdateState state, QSet<QnUuid> targets = {});

private:
    QScopedPointer<Ui::MultiServerUpdatesWidget> ui;

    QScopedPointer<QMenu> m_selectUpdateTypeMenu;
    QScopedPointer<QMenu> m_autoCheckMenu;
    QScopedPointer<QMenu> m_manualCheckMenu;

    // UI control flags. We run loadDataToUI periodically and check for this flags.
    bool m_updateLocalStateChanged = true;
    bool m_updateRemoteStateChanged = true;
    bool m_latestVersionChanged = true;
    // Flag shows that we have an update.
    bool m_haveValidUpdate = false;
    bool m_autoCheckUpdate = false;
    bool m_showStorageSettings = false;

    // It will enable additional column for server status
    // and a label with internal widget states
    bool m_showDebugData = false;

    UpdateCheckMode m_updateSourceMode = UpdateCheckMode::internet;

    std::shared_ptr<ServerUpdateTool> m_updatesTool;
    std::shared_ptr<ServerUpdatesModel> m_updatesModel;
    std::unique_ptr<QnSortedServerUpdatesModel> m_sortedModel;
    std::unique_ptr<ServerStatusItemDelegate> m_statusItemDelegate;

    // ServerUpdateTool promises this.
    std::future<ServerUpdateTool::UpdateContents> m_updateCheck;

    // TODO: We could move it inside serverUpdateTool
    ServerUpdateTool::UpdateContents m_updateInfo;
    QString m_updateCheckError;
    // We get this version either from internet, or zip package.
    nx::utils::SoftwareVersion m_availableVersion;
    nx::utils::SoftwareVersion m_targetVersion;

    WidgetUpdateState m_updateStateCurrent = WidgetUpdateState::initial;
    WidgetUpdateState m_updateStateTarget = WidgetUpdateState::initial;

    // Selected changeset from 'specific build' mode.
    QString m_targetChangeset;
    // Watchdog timer for the case when update has taken too long.
    std::unique_ptr<QTimer> m_longUpdateWarningTimer = nullptr;

    // Timer for updating interal state and synching it with UI.
    std::unique_ptr<QTimer> m_stateCheckTimer = nullptr;

    qint64 m_lastAutoUpdateCheck = 0;

    // Id of the progress notification at the right panel.
    QnUuid m_rightPanelDownloadProgress;

    // TODO: We used this sets when we commanded each server directly. So update state could diverge.
    // Right now we do not need to track that much.
    // This sets are changed every time we are initiating some update action.
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
