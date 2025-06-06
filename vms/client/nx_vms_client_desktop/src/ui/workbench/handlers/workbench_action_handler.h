// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <optional>

#include <QtCore/QBuffer>
#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QUrl>
#include <QtWidgets/QDialogButtonBox>

#include <api/model/camera_list_reply.h>
#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/network/http/async_http_client_reply.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>
#include <nx/vms/client/core/common/data/motion_selection.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/system_administration/dialogs/integrations_dialog.h>
#include <recording/time_period.h>
#include <ui/dialogs/audit_log_dialog.h>
#include <ui/dialogs/camera_list_dialog.h>
#include <ui/dialogs/search_bookmarks_dialog.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QAction;
class QMenu;

class QnProgressDialog;
class QnResourcePool;
class QnWorkbenchContext;
class ActionHandler;
class QnPopupCollectionWidget;
class QnWorkbenchNotificationsHandler;
class QnAdjustVideoDialog;
class QnSystemAdministrationDialog;

namespace nx::vms::client::desktop {

class AdvancedUpdateSettingsDialog;
class SceneBanner;
class AdvancedSearchDialog;
class IntegrationsDialog;

namespace rules { class RulesDialog; }

namespace ui {

namespace experimental { class MainWindow; }

namespace workbench {

using namespace std::literals;

enum class AppClosingMode {
    Exit,
    CloseAll,
    External //< Requested to close from another client instance
};

// TODO: #sivanov Split this class into several handlers, group actions by handler. E.g. screen
// recording should definitely be spun off.
/**
* This class implements logic for client actions.
*/
class ActionHandler : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    using MoveResourcesResultFunc =
        std::function<void(QnSharedResourcePointerList<QnResource> moved)>;

public:
    ActionHandler(QObject *parent = nullptr);
    virtual ~ActionHandler();

    void moveResourcesToServer(
        const QnResourceList& resources,
        const QnMediaServerResourcePtr& server,
        MoveResourcesResultFunc resultCallback);

protected:
    struct AddToLayoutParams
    {
        std::optional<QPointF> position;
        std::optional<QSizeF> size;
        QRectF zoomWindow;
        nx::Uuid zoomUuid;
        std::chrono::milliseconds time = -1ms;
        bool paused = false;
        qreal speed = 0.0;
        QColor frameDistinctionColor;
        bool zoomWindowRectangleVisible = true;
        qreal rotation = 0.0;
        nx::vms::api::ImageCorrectionData contrastParams;
        nx::vms::api::dewarping::ViewData dewarpingParams;
        QnTimePeriod timelineWindow;
        QnTimePeriod timelineSelection;
        nx::vms::client::core::MotionSelection motionSelection;
        QRectF analyticsSelection;
        bool displayRoi = true;
        bool displayAnalyticsObjects = false;
        bool displayHotspots = true;

        /** Add item as already zoomed. */
        bool zoomed = false;
    };

    void addToLayout(
        const core::LayoutResourcePtr& layout,
        const QnResourcePtr& resource,
        const AddToLayoutParams& params);
    void addToLayout(
        const core::LayoutResourcePtr& layout,
        const QnResourceList& resources,
        const AddToLayoutParams& params);
    void addToLayout(
        const core::LayoutResourcePtr& layout,
        const QList<QnMediaResourcePtr>& resources,
        const AddToLayoutParams& params);
    void addToLayout(
        const core::LayoutResourcePtr& layout,
        const QList<QString>& files,
        const AddToLayoutParams& params);

    QnResourceList addToResourcePool(const QString &file) const;
    QnResourceList addToResourcePool(const QList<QString> &files) const;

    void openResourcesInNewWindow(const QnResourceList &resources);

    void rotateItems(int degrees);

    void setCurrentLayoutCellSpacing(core::CellSpacing spacing);

    QnCameraListDialog *cameraListDialog() const;

    QnSystemAdministrationDialog *systemAdministrationDialog() const;

    std::optional<QnVirtualCameraResourceList> checkCameraList(
        bool success,
        int handle,
        const QnCameraListReply& reply);

protected:
    void at_context_userChanged(const QnUserResourcePtr &user);

    void at_workbench_cellSpacingChanged();

    void at_nextLayoutAction_triggered();
    void at_previousLayoutAction_triggered();

    void at_openAction_triggered();
    void at_openInLayoutAction_triggered();

    void at_openInCurrentLayoutAction_triggered();
    void at_openInNewWindowAction_triggered();

    void at_openCurrentLayoutInNewWindowAction_triggered();
    void at_openNewWindowAction_triggered();
    void at_openWelcomeScreenAction_triggered();
    void at_reviewShowreelInNewWindowAction_triggered();

    void at_moveCameraAction_triggered();
    void at_dropResourcesAction_triggered();
    void at_openFileAction_triggered();
    void at_openFolderAction_triggered();
    void at_aboutAction_triggered();
    void at_openBookmarksSearchAction_triggered();
    void openIntegrationsActionTriggered();
    void at_openAuditLogAction_triggered();
    void at_cameraListAction_triggered();

    void at_webAdminAction_triggered();
    void at_webClientAction_triggered();

    void at_mediaFileSettingsAction_triggered();
    void replaceCameraActionTriggered();
    void undoReplaceCameraActionTriggered();
    void at_cameraIssuesAction_triggered();
    void at_cameraDiagnosticsAction_triggered();
    void at_serverLogsAction_triggered();
    void at_serverIssuesAction_triggered();
    void at_pingAction_triggered();
    void at_thumbnailsSearchAction_triggered();

    void at_openInFolderAction_triggered();
    void at_deleteFromDiskAction_triggered();

    void at_renameAction_triggered();
    void at_removeFromServerAction_triggered();

    void at_adjustVideoAction_triggered();
    void at_beforeExitAction_triggered();

    void at_createZoomWindowAction_triggered();

    void at_setAsBackgroundAction_triggered();
    void setCurrentLayoutBackground(const QString &filename);

    void at_workbench_itemChanged(Qn::ItemRole role);

    void at_whatsThisAction_triggered();

    void at_escapeHotkeyAction_triggered();

    void at_browseUrlAction_triggered();

    void at_betaVersionMessageAction_triggered();

    void at_queueAppRestartAction_triggered();
    void at_selectTimeServerAction_triggered();

    void at_changeDefaultCameraPassword_triggered();

    void at_openNewScene_triggered();

    void at_systemAdministrationAction_triggered();

    void at_advancedUpdateSettingsAction_triggered();

    void at_jumpToTimeAction_triggered();

    void at_goToLayoutItemAction_triggered();

    void at_itemMuteAction_triggered();
    void at_itemUnmuteAction_triggered();

    void at_clientCommandRequested(SharedMemoryData::Command command, const QByteArray& data);

    void at_saveSessionState_triggered();
    void at_restoreSessionState_triggered();
    void at_deleteSessionState_triggered();
    void showSessionSavedBanner();

    void at_openAdvancedSearchDialog_triggered();

    void at_openImportFromDevicesDialog_triggered();

    void handleBetaUpdateWarning();

    void replaceLayoutItemActionTriggered();

    void cameraAuthenticationActionTriggered();

private:
    void showSingleCameraErrorMessage(const QString& explanation = QString());
    void showMultipleCamerasErrorMessage(
        int totalCameras,
        const QnVirtualCameraResourceList& camerasWithError,
        const QString& explanation = QString());

    void changeDefaultPasswords(
        const QString& previousPassword,
        const QnVirtualCameraResourceList& cameras,
        bool forceShowCamerasList);
    void onDefaultPasswordChanged(
        const QString& password,
        const QnVirtualCameraResourceList& cameras,
        bool forceShowCamerasList);

    void notifyAboutUpdate();

    void openFailoverPriorityDialog();
    void openSystemAdministrationDialog(int page, const QUrl& url = {});
    void openLocalSettingsDialog(int page);

    QnAdjustVideoDialog* adjustVideoDialog();

    /** Check if resource can be safely renamed to the new name. */
    bool validateResourceName(const QnResourcePtr &resource, const QString &newName) const;

    void confirmAnalyticsStorageLocation();

    void deleteDialogs();

    void closeAllWindows();
    void closeApplication();
    void doCloseApplication(AppClosingMode mode);

    void renameLocalFile(const QnResourcePtr& resource, const QString &newName);

    void openInBrowserDirectly(const QnMediaServerResourcePtr& server,
        const QString& path = QString(), const QString& fragment = QString());
    void openInBrowser(const QnMediaServerResourcePtr& server,
        const QString& path = QString(), const QString& fragment = QString());
    void at_nonceReceived(QnAsyncHttpClientReply* client);

    enum class AuthMethod { queryParam, bearerToken };
    void openInBrowser(const QnMediaServerResourcePtr& server,
        nx::Url targetUrl, std::string auth, AuthMethod authMethod) const;

    void muteUnmuteWidgets(const QnResourceWidgetList& widgets, bool muted) const;

private:
    QPointer<QnSearchBookmarksDialog> m_searchBookmarksDialog;
    QPointer<QnAuditLogDialog> m_auditLogDialog;
    QPointer<QnCameraListDialog> m_cameraListDialog;
    QPointer<QnAdjustVideoDialog> m_adjustVideoDialog;
    QPointer<QnSystemAdministrationDialog> m_systemAdministrationDialog;
    QPointer<AdvancedUpdateSettingsDialog> m_advancedUpdateSettingsDialog;
    QPointer<AdvancedSearchDialog> m_advancedSearchDialog;
    QPointer<rules::RulesDialog> m_rulesDialog;
    std::unique_ptr<IntegrationsDialog> m_integrationsDialog;
    std::unique_ptr<experimental::MainWindow> m_mainWindow;

    QQueue<QnMediaResourcePtr> m_layoutExportResources;
    QString m_layoutFileName;
    QnTimePeriod m_exportPeriod;
    QnLayoutResourcePtr m_exportLayout;
    QnStorageResourcePtr m_exportStorage;

    struct CameraMovingInfo
    {
        CameraMovingInfo() {}
        CameraMovingInfo(const QnVirtualCameraResourceList& cameras, const QnMediaServerResourcePtr& dstServer) : cameras(cameras), dstServer(dstServer) {}
        QnVirtualCameraResourceList cameras;
        QnMediaServerResourcePtr dstServer;
    };
    QMap<int, CameraMovingInfo> m_awaitingMoveCameras;

    struct ServerRequest
    {
        QnMediaServerResourcePtr server;
        nx::Url url;
    };

    std::multimap<nx::Url, ServerRequest> m_serverRequests;
    QPointer<nx::vms::client::desktop::SceneBanner> m_layoutIsFullMessage;
    QPointer<nx::vms::client::desktop::SceneBanner> m_accessDeniedMessage;

    QList<menu::Parameters> m_queuedDropParameters;
    bool m_inDropResourcesAction = false;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
