#pragma once

#include <atomic>

#include <QtCore/QBuffer>
#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QUrl>

#include <QtWidgets/QDialogButtonBox>

#include <api/app_server_connection.h>

#include <nx_ec/ec_api.h>

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/event_log_dialog.h>
#include <ui/dialogs/camera_list_dialog.h>
#include <ui/dialogs/search_bookmarks_dialog.h>
#include "ui/dialogs/audit_log_dialog.h"

#include "api/model/camera_list_reply.h"
#include <nx/network/http/async_http_client_reply.h>

#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>

class QAction;
class QMenu;

class QnProgressDialog;
class QnResourcePool;
class QnWorkbench;
class QnWorkbenchContext;
class QnWorkbenchSynchronizer;
class QnWorkbenchLayoutSnapshotManager;
class ActionHandler;
class QnBusinessRulesDialog;
class QnPopupCollectionWidget;
class QnWorkbenchNotificationsHandler;
class QnAdjustVideoDialog;
class QnSystemAdministrationDialog;
class QnGraphicsMessageBox;

namespace nx::vms::client::desktop {

class MimeData;

namespace ui {


namespace experimental { class MainWindow; }

namespace workbench {

// TODO: #Elric split this class into several handlers, group actions by handler. E.g. screen recording should definitely be spun off.
/**
* This class implements logic for client actions.
*/
class ActionHandler : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    ActionHandler(QObject *parent = NULL);
    virtual ~ActionHandler();

protected:
    struct AddToLayoutParams {
        bool usePosition = false;
        QPointF position;
        QRectF zoomWindow;
        QnUuid zoomUuid;
        qint64 time = -1;
        QColor frameDistinctionColor;
        bool zoomWindowRectangleVisible = true;
        qreal rotation = 0.0;
        nx::vms::api::ImageCorrectionData contrastParams;
        nx::vms::api::DewarpingData dewarpingParams;
    };

    void addToLayout(const QnLayoutResourcePtr &layout, const QnResourcePtr &resource, const AddToLayoutParams &params);
    void addToLayout(const QnLayoutResourcePtr &layout, const QnResourceList &resources, const AddToLayoutParams &params);
    void addToLayout(const QnLayoutResourcePtr &layout, const QList<QnMediaResourcePtr>& resources, const AddToLayoutParams &params);
    void addToLayout(const QnLayoutResourcePtr &layout, const QList<QString> &files, const AddToLayoutParams &params);

    QnResourceList addToResourcePool(const QString &file) const;
    QnResourceList addToResourcePool(const QList<QString> &files) const;

    void openResourcesInNewWindow(const QnResourceList &resources);

    void openNewWindow(const QStringList &args);

    void rotateItems(int degrees);

    void setCurrentLayoutCellSpacing(Qn::CellSpacing spacing);

    QnBusinessRulesDialog *businessRulesDialog() const;

    QnEventLogDialog *businessEventsLogDialog() const;

    QnCameraListDialog *cameraListDialog() const;

    QnSystemAdministrationDialog *systemAdministrationDialog() const;

protected slots:
    void at_context_userChanged(const QnUserResourcePtr &user);

    void at_workbench_cellSpacingChanged();

    void at_nextLayoutAction_triggered();
    void at_previousLayoutAction_triggered();

    void at_openInLayoutAction_triggered();

    void at_openInCurrentLayoutAction_triggered();
    void at_openInNewTabAction_triggered();
    void at_openInNewWindowAction_triggered();

    void at_openCurrentLayoutInNewWindowAction_triggered();
    void at_openNewWindowAction_triggered();
    void at_reviewLayoutTourInNewWindowAction_triggered();

    void at_moveCameraAction_triggered();
    void at_dropResourcesAction_triggered();
    void at_openFileAction_triggered();
    void at_openFolderAction_triggered();
    void at_aboutAction_triggered();
    void at_businessEventsAction_triggered();
    void at_openBusinessRulesAction_triggered();
    void at_openBookmarksSearchAction_triggered();
    void at_openBusinessLogAction_triggered();
    void at_openAuditLogAction_triggered();
    void at_cameraListAction_triggered();

    void at_webAdminAction_triggered();
    void at_webClientAction_triggered();

    void at_mediaFileSettingsAction_triggered();
    void at_cameraIssuesAction_triggered();
    void at_cameraBusinessRulesAction_triggered();
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

    void at_scheduleWatcher_scheduleEnabledChanged();
    void at_togglePanicModeAction_toggled(bool checked);

    void at_workbench_itemChanged(Qn::ItemRole role);

    void at_whatsThisAction_triggered();

    void at_escapeHotkeyAction_triggered();

    void at_browseUrlAction_triggered();

    void at_versionMismatchMessageAction_triggered();

    void at_betaVersionMessageAction_triggered();

    void at_queueAppRestartAction_triggered();
    void at_selectTimeServerAction_triggered();
    void at_cameraListChecked(int status, const QnCameraListReply& reply, int handle);

    void at_convertCameraToEntropix_triggered();

    void at_changeDefaultCameraPassword_triggered();

    void at_openNewScene_triggered();

    void at_systemAdministrationAction_triggered();

    void at_jumpToTimeAction_triggered();

    void at_goToLayoutItemAction_triggered();

private:
    void showSingleCameraErrorMessage(const QString& explanation = QString());
    void showMultipleCamerasErrorMessage(
        int totalCameras,
        const QnVirtualCameraResourceList& camerasWithError,
        const QString& explanation = QString());

    void changeDefaultPasswords(
        const QString& previousPassword,
        const QnVirtualCameraResourceList& cameras,
        bool showSingleCamera);

    void notifyAboutUpdate();

    void openFailoverPriorityDialog();
    void openBackupCamerasDialog();
    void openSystemAdministrationDialog(int page, const QUrl& url = {});
    void openLocalSettingsDialog(int page);

    QnAdjustVideoDialog* adjustVideoDialog();

    /** Check if resource can be safely renamed to the new name. */
    bool validateResourceName(const QnResourcePtr &resource, const QString &newName) const;

    /** Check if statistics reporting is allowed in the system - and ask user if still not defined. */
    void checkIfStatisticsReportAllowed();

    void deleteDialogs();

    void closeApplication(bool force = false);

    qint64 getFirstBookmarkTimeMs();

    void renameLocalFile(const QnResourcePtr& resource, const QString &newName);

    void openInBrowserDirectly(const QnMediaServerResourcePtr& server,
        const QString& path, const QString& fragment = QString());
    void openInBrowser(const QnMediaServerResourcePtr& server,
        const QString& path, const QString& fragment = QString());
    void at_nonceReceived(QnAsyncHttpClientReply* client);

private:
    QPointer<QnBusinessRulesDialog> m_businessRulesDialog;
    QPointer<QnEventLogDialog> m_businessEventsLogDialog;
    QPointer<QnSearchBookmarksDialog> m_searchBookmarksDialog;
    QPointer<QnAuditLogDialog> m_auditLogDialog;
    QPointer<QnCameraListDialog> m_cameraListDialog;
    QPointer<QnAdjustVideoDialog> m_adjustVideoDialog;
    QPointer<QnSystemAdministrationDialog> m_systemAdministrationDialog;

    QPointer<experimental::MainWindow> m_mainWindow;

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
        nx::utils::Url url;
    };

    std::multimap<nx::utils::Url, ServerRequest> m_serverRequests;
    QPointer<QnGraphicsMessageBox> m_layoutIsFullMessage;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
