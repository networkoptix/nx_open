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
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/update/common_update_manager.h>
#include <update/updates_common.h>

#include "server_update_tool.h"
#include "client_update_tool.h"

struct QnLowFreeSpaceWarning;

namespace Ui { class MultiServerUpdatesWidget; }

namespace nx::vms::client::desktop {

class ServerUpdatesModel;
class SortedPeerUpdatesModel;
class ServerStatusItemDelegate;
struct UpdateItem;

/**
 * Deals with update for multiple servers.
 * It is spawned as a tab for System Administraton menu.
 */
class MultiServerUpdatesWidget:
    public QnAbstractPreferencesWidget,
    public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;
    using LocalStatusCode = nx::update::Status::Code;
    using UpdateSourceType = nx::update::UpdateSourceType;

public:
    MultiServerUpdatesWidget(QWidget* parent = nullptr);
    virtual ~MultiServerUpdatesWidget() override;

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

    virtual void applyChanges() override;
    virtual void discardChanges() override;

    /** Updates UI state to match internal state. */
    virtual void loadDataToUi() override;

    virtual bool hasChanges() const override;

    virtual bool canDiscardChanges() const override;

protected:
    /** This one is called by timer periodically. */
    void atUpdateCurrentState();
    void atModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles);
    void atStartUpdateAction();
    bool atCancelCurrentAction();
    void atServerPackageDownloaded(const nx::update::Package& package);
    void atServerPackageDownloadFailed(const nx::update::Package& package, const QString& error);
    void atCloudSettingsChanged();

    void clearUpdateInfo();
    void pickLocalFile();
    void pickSpecificBuild();

private:
    /**
     * General state for a widget.
     * It extends update state from the servers, uploader and client update.
     */
    enum class WidgetUpdateState
    {
        /** We have no information about remote state right now. */
        initial,
        /**
         * We have obtained update info from the internet, but we need to
         * GET /ec2/updateInformation from the servers, to decide which state we should move to.
         */
        checkingServers,
        /**
         * We have obtained some state from the servers. We can do some actions now.
         * Next action depends on m_updateSourceMode, and whether the update
         * is available for picked update source.
         */
        ready,
        /** We have issued a command to remote servers to start downloading the updates. */
        downloading,
        /** Pushing local update package to the servers. */
        pushing,
        /** Some servers have downloaded update data and ready to install it. */
        readyInstall,
        /** Some servers are installing an update. */
        installing,
        /** Some servers are installing an update, but it took too long. */
        installingStalled,
        /** Installation process is complete. */
        complete,
    };

    static QString toString(nx::update::UpdateSourceType mode);
    static QString toString(WidgetUpdateState state);
    static QString toString(LocalStatusCode stage);
    static QString toString(ServerUpdateTool::OfflineUpdateState state);

    void setUpdateSourceMode(nx::update::UpdateSourceType mode);

    void initDropdownActions();
    void initDownloadActions();

    void setAutoUpdateCheckMode(bool mode);
    void autoCheckForUpdates();
    void checkForInternetUpdates(bool initial = false);

    /**
     * Describes all possible display modes for update version.
     * Variations for build number:
     *  - Full build number, "4.0.0.20000".
     *  - Specific build number "10821". Appears when we waiting for response for specific build.
     *  - No version avalilable, "-----". Often happens if we have Error::networkError
     *      or Error::httpError report. Can happen when source=internet
     */
    struct VersionReport
    {
        bool hasLatestVersion = false;
        bool checking = false;
        QString version;
        /** Status messages. It is displayed under version when something went wrong. */
        QStringList statusMessages;
        /** Modes for displaying version number. */
        enum class VersionMode
        {
            /** No version is available. We display '-----'.*/
            empty,
            /** Only build number is displayed. */
            build,
            /** Display full version. */
            full,
        };

        VersionMode versionMode = VersionMode::full;
        enum class HighlightMode
        {
            regular,
            bright,
            red,
        };

        HighlightMode versionHighlight = HighlightMode::regular;
        HighlightMode statusHighlight = HighlightMode::regular;
    };

    VersionReport calculateUpdateVersionReport(const nx::update::UpdateContents& contents);

    void syncVersionReport(const VersionReport& report);

    /**
     * UI synhronization. This functions are ment to be called from loadDataToUi.
     * Do not call them from anywhere else.
     */
    void syncUpdateCheckToUi();
    void syncRemoteUpdateStateToUi();
    void syncProgress();
    void syncStatusVisibility();

    ServerUpdateTool::ProgressInfo calculateActionProgress() const;

    bool processRemoteChanges(bool force = false);
    /** Part of processRemoteChanges FSM processor. */
    void processRemoteInitialState();
    void processRemoteUpdateInformation();
    void processRemoteDownloading();
    void processRemoteInstalling();

    void updateInfoVisibility();
    void updateTopPanelLayout();
    bool isChecking() const;

    bool processUploaderChanges(bool force = false);

    void closePanelNotifications();

    /** Advances UI FSM towards selected state. */
    void setTargetState(WidgetUpdateState state, QSet<QnUuid> targets = {});
    void completeInstallation(bool clientUpdated);
    static bool stateHasProgress(WidgetUpdateState state);
    void syncDebugInfoToUi();

private:
    QScopedPointer<Ui::MultiServerUpdatesWidget> ui;
    QScopedPointer<QMenu> m_selectUpdateTypeMenu;
    QScopedPointer<QMenu> m_autoCheckMenu;
    QScopedPointer<QMenu> m_manualCheckMenu;

    /** UI control flags. We run loadDataToUI periodically and check for this flags. */
    bool m_updateLocalStateChanged = true;
    bool m_updateRemoteStateChanged = true;
    /** Flag shows that we have an update. */
    bool m_haveValidUpdate = false;
    bool m_autoCheckUpdate = false;
    bool m_showStorageSettings = false;
    bool m_gotServerData = false;

    /**
     * It will enable additional column for server status
     * and a label with internal widget states.
     */
    bool m_showDebugData = false;

    nx::update::UpdateSourceType m_updateSourceMode = nx::update::UpdateSourceType::internet;

    std::unique_ptr<ServerUpdateTool> m_serverUpdateTool;
    std::unique_ptr<ClientUpdateTool> m_clientUpdateTool;
    std::shared_ptr<ServerUpdatesModel> m_updatesModel;
    std::shared_ptr<PeerStateTracker> m_stateTracker;
    std::unique_ptr<SortedPeerUpdatesModel> m_sortedModel;
    std::unique_ptr<ServerStatusItemDelegate> m_statusItemDelegate;

    /** ServerUpdateTool promises this. */
    std::future<nx::update::UpdateContents> m_updateCheck;

    /** This promise is used to get update info from mediaservers. */
    std::future<nx::update::UpdateContents> m_serverUpdateCheck;

    std::future<std::vector<nx::update::Status>> m_serverStatusCheck;

    nx::update::UpdateContents m_updateInfo;
    VersionReport m_updateReport;
    nx::utils::SoftwareVersion m_targetVersion;

    WidgetUpdateState m_widgetState = WidgetUpdateState::initial;

    /** Selected changeset from 'specific build' mode. */
    QString m_targetChangeset;

    /** Watchdog timer for the case when update has taken too long. */
    std::unique_ptr<QTimer> m_longUpdateWarningTimer = nullptr;

    /** Timer for updating interal state and synching it with UI. */
    std::unique_ptr<QTimer> m_stateCheckTimer = nullptr;

    qint64 m_lastAutoUpdateCheck = 0;

    /** Id of the progress notification at the right panel. */
    QnUuid m_rightPanelDownloadProgress;

    /**
     * This sets are changed every time we are initiating some update action.
     * Set of servers that are currently active.
     */
    QSet<QnUuid> m_peersActive;
    /** Set of servers that are used for the current task. */
    QSet<QnUuid> m_peersIssued;
    /** A set of servers that have completed current task. */
    QSet<QnUuid> m_peersComplete;
    /** A set of servers that have failed current task. */
    QSet<QnUuid> m_peersFailed;
};

} // namespace nx::vms::client::desktop
