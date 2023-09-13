// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/common/id.h>

#include "client_update_tool.h"
#include "server_status_delegate.h"
#include "server_update_tool.h"

namespace Ui { class MultiServerUpdatesWidget; }

namespace nx::vms::client::desktop {

class ServerUpdatesModel;
class SortedPeerUpdatesModel;
struct UpdateItem;

/**
 * Deals with update for multiple servers.
 * It is spawned as a tab for System Administraton menu.
 */
class NX_VMS_CLIENT_DESKTOP_API MultiServerUpdatesWidget:
    public QnAbstractPreferencesWidget,
    public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;
    using LocalStatusCode = nx::vms::common::update::Status::Code;

public:
    MultiServerUpdatesWidget(QWidget* parent = nullptr);
    virtual ~MultiServerUpdatesWidget() override;

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;
    virtual void discardChanges() override;

    /** Updates UI state to match internal state. */
    virtual void loadDataToUi() override;

    /**
     * Report for picked update version. Describes all possible display modes for update version.
     * It contains all data necessary to show at the top panel of the dialog.
     *
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
        QString alreadyInstalledMessage;
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
        bool notEnoughSpace = false;
        enum class HighlightMode
        {
            regular,
            bright,
            red,
        };

        HighlightMode versionHighlight = HighlightMode::regular;
        HighlightMode statusHighlight = HighlightMode::regular;

        bool operator==(const VersionReport& another) const;
    };

    /** Incapsulates all the state for control panel. */
    struct ControlPanelState
    {
        bool actionEnabled = true;
        QString actionCaption;
        QStringList actionTooltips;

        bool showManualDownload = false;
        bool cancelEnabled = true;
        enum class PanelMode
        {
            /** Showing 'download, cancel' buttons. */
            control,
            /** Showing process panel. */
            process,
            /** Showing empty panel. */
            empty,
        };
        PanelMode panelMode = PanelMode::control;

        QString progressCaption;
        QString cancelProgressCaption;
        bool cancelProgressEnabled = false;
        int progressMinimum = 0;
        int progressMaximum = 0;
        int progress = 0;

        bool operator==(const ControlPanelState& another) const;
    };

    /**
     * Storage wrapper with revision and change tracking.
     * It addresses the problem of checking whether UI state should be updated after
     * the changes in internal (half UI) state:
     *   business logic -> half UI state -> rendered state
     * It does not address reverse changes from UI state to business logic.
     *
     * RevisionStorage requires `operator ==` to be available for the type `Data`.
     */
    template<class Data>
    struct RevisionStorage
    {
        RevisionStorage(const Data& data):
            currentData(data),
            referenceData(data)
        {
        }

        RevisionStorage() = default;

        /** Checks if there are changes to the data. */
        bool changed() const
        {
            return referenceRevision != revision || !(referenceData == currentData);
        }

        void bumpRevision()
        {
            // We do not bump revision if there are changes already.
            if (referenceRevision == revision)
                revision++;
        }

        /** Accept changes to the data. It should be used after new data was rendered to UI. */
        void acceptChanges()
        {
            referenceData = currentData;
            referenceRevision = revision;
        }

        /** Rollback the changes. */
        void rollback()
        {
            currentData = referenceData;
            revision = referenceRevision;
        }

        operator Data&()
        {
            return currentData;
        }

        operator const Data&() const
        {
            return currentData;
        }

        Data& operator()()
        {
            return currentData;
        }

        const Data& operator()() const
        {
            return currentData;
        }

        Data* operator->()
        {
            return &currentData;
        }

        const Data* operator->() const
        {
            return &currentData;
        }

        bool operator==(const Data& other) const
        {
            return currentData == other;
        }

        bool operator!=(const Data& other) const
        {
            return currentData != other;
        }

        Data& operator=(const Data& other)
        {
            if (currentData == other)
                return currentData;
            currentData = other;
            revision++;
            return currentData;
        }

        template<class Type> bool compareAndSet(Type& to, const Type& from)
        {
            /**
             * If it was python, I would just override __dict__ method instead and bump revision
             * if data has changed. But it is C++ and there's no way to make it look good.
             */
            if (from == to)
                return false;
            to = from;
            bumpRevision();
            return true;
        }

        /*
        template<class Type,
            typename std::enable_if<std::is_class<Data>::value, Type>::type* = nullptr>
        bool compareAndSet(Type Data::*to, const Type& from)
        {
            Type& value = currentData.*to;
            if (from == value)
                return false;
            value = from;
            bumpRevision();
            return true;
        }*/

    private:
        Data currentData;
        Data referenceData;
        int revision = 1;
        int referenceRevision = 0;
    };

    /** Generates update report for a picked update contents. */
    static VersionReport calculateUpdateVersionReport(
        const UpdateContents& contents,
        QnUuid clientId);

    bool checkSpaceRequirements(const UpdateContents& contents) const;

protected:
    /**
     * This one is called by timer periodically.
     * It checks changes in remote and local state, and calls update for UI if there are any.
     */
    void atUpdateCurrentState();

    /** Handler for a separate timer for checking installation status. We check it less often. */
    void atCheckInstallState();
    void atModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles);
    void atStartUpdateAction();
    /** Called by clicking 'cancel' button. */
    bool atCancelCurrentAction();

    /** Called when server responds to /ec2/startUpdate. Connected to ServerUpdateToool. */
    void atStartUpdateComplete(bool success, const QString& error);
    /** Called when server responds to /ec2/cancelUpdate. Connected to ServerUpdateToool. */
    void atCancelUpdateComplete(bool success, const QString& error);
    /** Called when server responds to /ec2/finishUpdate. Connected to ServerUpdateToool. */
    void atFinishUpdateComplete(bool success, const QString& error);
    /** Called when server responds to /ec2/installUpdate. Connected to ServerUpdateToool. */
    void atStartInstallComplete(bool success, const QString& error);

    /**
     * Called when ServerUpdateTool finishes downloading package for mediaserver without
     * internet.
     */
    void atServerPackageDownloaded(const nx::vms::update::Package& package);
    void atServerPackageDownloadFailed(const nx::vms::update::Package& package, const QString& error);

    void atServerConfigurationChanged(std::shared_ptr<UpdateItem> item);

    void clearUpdateInfo();
    void pickLocalFile();
    void pickSpecificBuild();

    void openAdvancedSettings();

private:
    /**
     * General state for a widget.
     * It extends update state from the servers, uploader and client update.
     */
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(WidgetUpdateState,
        /** We have no information about remote state right now. */
        initial,
        /**
         * We have obtained update info from the Internet, but we need to
         * GET /ec2/updateInformation from the servers, to decide which state we should move to.
         */
        initialCheck,
        /**
         * We have obtained some state from the servers. We can do some actions now.
         * Next action depends on m_updateSourceMode, and whether the update
         * is available for picked update source.
         */
        ready,
        /** We have sent /ec2/startUpdate and waiting for response. */
        startingDownload,
        /** Mediaservers have started downloading update packages. */
        downloading,
        /**
         * Waiting server to respond to /ec2/cancelUpdate from 'downloading' or 'pushing'
         * state.
         */
        cancelingDownload,
        /** Some servers have downloaded update data and ready to install it. */
        readyInstall,
        /** Sent request /ec2/cancelUpdate and waiting for response. */
        cancelingReadyInstall,
        /** Sent request /ec2/installUpdate and waiting for confirmation */
        startingInstall,
        /** Some servers are installing an update. */
        installing,
        /** Some servers are installing an update, but it took too long. */
        installingStalled,
        /** Waiting server to respond to /ec2/finishUpdate */
        finishingInstall,
        /** Installation process is complete. */
        complete
    )

    static QString toString(UpdateSourceType mode);
    static QString toString(WidgetUpdateState state);
    static QString toString(ServerUpdateTool::OfflineUpdateState state);

    /**
     * Changes source for getting update.
     * It will try to skip any changes if we picking the same mode.
     * @param mode - desired mode.
     * @param force - should we force mode changes, even if mode is the same.
     */
    void setUpdateSourceMode(UpdateSourceType mode, bool force = false);

    void initDropdownActions();
    void initDownloadActions();

    void autoCheckForUpdates();
    void checkForInternetUpdates(bool initial = false);
    void recheckSpecificBuild();

    /**
     * Repeat validation process for current update information. Will work only in 'Ready' state.
     */
    void repeatUpdateValidation();
    void syncVersionReport(const VersionReport& report);

    /**
     * UI synhronization. This functions are ment to be called from loadDataToUi.
     * Do not call them from anywhere else.
     */
    void syncUpdateCheckToUi();
    void syncRemoteUpdateStateToUi();
    void syncProgress();
    void syncVersionInfoVisibility();

    ServerStatusItemDelegate::StatusMode calculateStatusColumnVisibility() const;
    ServerUpdateTool::ProgressInfo calculateActionProgress() const;

    bool processRemoteChanges();
    /** Part of processRemoteChanges FSM processor. */
    void processInitialState();
    void processInitialCheckState();
    void processDownloadingState();
    void processReadyInstallState();
    void processInstallingState();

    bool isChecking() const;
    bool hasLatestVersion() const;
    bool hasActiveUpdate() const;
    bool hasPendingUiChanges() const;

    bool processUploaderChanges(bool force = false);

    void closePanelNotifications();

    /** Advances UI FSM towards selected state. */
    void setTargetState(WidgetUpdateState state, const QSet<QnUuid>& targets = {},
        bool runCommands = true);
    void completeClientInstallation(bool clientUpdated);
    static bool stateHasProgress(WidgetUpdateState state);
    void syncDebugInfoToUi();
    /**
     * Sets current update target.
     * It changes m_updateInfo and updates UI states according to the report.
     * @param contents - update contents.
     * @param activeUpdate - true if mediaservers are already updating to this version.
     */
    void setUpdateTarget(const UpdateContents& contents, bool activeUpdate);

    QnUuid clientPeerId() const;

    void setDayWarningVisible(bool visible);
    void updateAlertBlock();

    /**
    * Generates URL for Upcombiner, a WEB service, that combines several update packages into
    * a single ZIP archive.
    */
    nx::utils::Url generateUpcombinerUrl() const;

    nx::utils::SoftwareVersion getMinimumComponentVersion() const;

private:
    QScopedPointer<Ui::MultiServerUpdatesWidget> ui;
    QScopedPointer<QMenu> m_selectUpdateTypeMenu;

    /** UI control flags. We run loadDataToUI periodically and check for this flags. */
    bool m_forceUiStateUpdate = true;
    bool m_updateRemoteStateChanged = true;
    bool m_showStorageSettings = false;

    /**
     * It will enable additional column for server status
     * and a label with internal widget states.
     */
    bool m_showDebugData = false;

    /** Finishing update when a user pressed "Finish update" before installation has finished. */
    bool m_finishingForcefully = false;

    QPointer<ServerUpdateTool> m_serverUpdateTool;
    std::unique_ptr<ClientUpdateTool> m_clientUpdateTool;
    std::shared_ptr<ServerUpdatesModel> m_updatesModel;
    std::shared_ptr<PeerStateTracker> m_stateTracker;
    std::unique_ptr<SortedPeerUpdatesModel> m_sortedModel;
    std::unique_ptr<ServerStatusItemDelegate> m_statusItemDelegate;
    std::unique_ptr<QnResourceItemDelegate> m_resourceNameDelegate;

    /** ServerUpdateTool promises this. */
    std::future<UpdateContents> m_updateCheck;
    /** This promise is used to get update info from mediaservers. */
    std::future<UpdateContents> m_serverUpdateCheck;

    /**
     * This promise is used to wait until ServerUpdateTool restores the state
     * for offline update package.
     */
    std::future<UpdateContents> m_offlineUpdateCheck;
    std::future<ServerUpdateTool::RemoteStatus> m_serverStatusCheck;

    UpdateContents m_updateInfo;
    RevisionStorage<UpdateSourceType> m_updateSourceMode = UpdateSourceType::internet;
    RevisionStorage<VersionReport> m_updateReport;
    RevisionStorage<ControlPanelState> m_controlPanelState;
    RevisionStorage<WidgetUpdateState> m_widgetState = WidgetUpdateState::initial;
    RevisionStorage<ServerStatusItemDelegate::StatusMode> m_statusColumnMode =
        ServerStatusItemDelegate::StatusMode::hidden;
    nx::utils::SoftwareVersion m_pickedSpecificBuild;

    /** Watchdog timer for the case when update has taken too long. */
    std::unique_ptr<QTimer> m_longUpdateWarningTimer;

    /** Timer for updating interal state and synching it with UI. */
    std::unique_ptr<QTimer> m_stateCheckTimer;

    /**
     * Timer for checking installation status. It works at different rate than
     * m_stateCheckTimer.
     */
    std::unique_ptr<QTimer> m_installCheckTimer;

    qint64 m_lastAutoUpdateCheck = 0;

    /** Id of the progress notification at the right panel. */
    QnUuid m_rightPanelDownloadProgress;

    bool m_dayWarningVisible = false;
};

} // namespace nx::vms::client::desktop
