#pragma once

#include <memory>   // for unique_ptr
#include <future>   // for the future
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_state_manager.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/id.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/update/update_check.h>
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
class UploadManager;

using Downloader = vms::common::p2p::downloader::Downloader;
using FileInformation = vms::common::p2p::downloader::FileInformation;

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
    bool canCancelUpdate() const;
    bool isUpdating() const;
    bool isChecking() const;

    //void at_updateFinished(const QnUpdateResult& result);
    //void at_selfUpdateFinished(const QnUpdateResult& result);

    void at_clientDownloadFinished();

    void at_modelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

protected:
    // Callback for timer events
    void at_stateCheckTimer();

    void at_clickUpdateServers();
    void at_clickCheckForUpdates();
    void at_startUpdateAction();
    bool at_cancelCurrentAction();

    void at_updateItemCommand(std::shared_ptr<UpdateItem> item);
    void at_downloaderStatusChanged(const FileInformation& fileInformation);

    void setModeLocalFile();
    void setModeSpecificBuild();
    void setModeLatestAvailable();
    void hideStatusColumns(bool value);

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

    static QString toString(UpdateSourceMode mode);
    static QString toString(WidgetUpdateState state);
    static QString toString(LocalStatusCode stage);

    void setUpdateSourceMode(UpdateSourceMode mode);

    void initDropdownActions();
    void initDownloadActions();

    void autoCheckForUpdates();
    void checkForRemoteUpdates();

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
    // Part of processRemoteChanges FSM processor.
    void processRemoteInitialState();

    void processRemoteDownloading(const ServerUpdateTool::RemoteStatus& remoteStatus);
    void processRemoteInstalling(const ServerUpdateTool::RemoteStatus& remoteStatus);

    bool processLegacyChanges(bool force = false);

    // Advances UI FSM towards selected state.
    void moveTowardsState(WidgetUpdateState state, QSet<QnUuid> targets = {});

private:
    QScopedPointer<Ui::MultiServerUpdatesWidget> ui;

    // UI control flags. We run loadDataToUI periodically and check for this flags.
    bool m_updateLocalStateChanged = true;
    bool m_updateRemoteStateChanged = true;
    bool m_latestVersionChanged = true;
    // Flag shows that we have an update.
    bool m_haveUpdate = false;
    bool m_haveClientUpdate = false;

    UpdateSourceMode m_updateSourceMode = UpdateSourceMode::LatestVersion;

    std::unique_ptr<ServerUpdateTool> m_updatesTool;
    std::unique_ptr<ServerUpdatesModel> m_updatesModel;
    std::unique_ptr<QnSortedServerUpdatesModel> m_sortedModel;
    std::unique_ptr<Downloader> m_downloader;
    // Downloader needs this strange thing.
    std::unique_ptr<vms::common::p2p::downloader::AbstractPeerManagerFactory> m_peerManagerFactory;
    // For pushing update package to the server swarm. Will be replaced by a p2p::Downloader.
    std::unique_ptr<UploadManager> m_uploadManager;

    struct UpdateCheckResult
    {
        nx::update::Information info;
        nx::update::InformationError error = nx::update::InformationError::noError;
    };
    std::future<UpdateCheckResult> m_updateCheck;
    nx::update::Information m_updateInfo;
    QString m_updateCheckError;
    // We get this version either from internet, or zip package.
    nx::utils::SoftwareVersion m_availableVersion;
    nx::utils::SoftwareVersion m_targetVersion;

    // Information for clent update.
    nx::update::Package m_clientUpdatePackage;

    WidgetUpdateState m_updateStateCurrent = WidgetUpdateState::Initial;
    WidgetUpdateState m_updateStateTarget = WidgetUpdateState::Initial;

    // Was ist das?
    QString m_targetChangeset;
    QString m_localFileName;

    // URL of update path.
    QUrl m_updateSourcePath;

    // Watchdog timer for the case when update has taken too long.
    std::unique_ptr<QTimer> m_longUpdateWarningTimer = nullptr;

    // Timer for updating interal state and synching it with UI.
    std::unique_ptr<QTimer> m_stateCheckTimer = nullptr;

    qint64 m_lastAutoUpdateCheck = 0;

    // TODO: We used this sets, when we commanded each server directly. So update state could diverge.
    // Right now we do not need to track that much
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
