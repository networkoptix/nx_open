#ifndef QN_WORKBENCH_ACTION_HANDLER_H
#define QN_WORKBENCH_ACTION_HANDLER_H

#include <QtCore/QBuffer>
#include <QtCore/QObject>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>

#include <core/ptz/item_dewarping_params.h>
#include <nx_ec/ec_api.h>

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <core/resource/resource_fwd.h>

#include <ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/event_log_dialog.h>
#include <ui/dialogs/camera_list_dialog.h>

#include <utils/color_space/image_correction.h>
#include "api/model/camera_list_reply.h"

class QAction;
class QMenu;

class QnProgressDialog;
class QnMimeData;
class QnResourcePool;
class QnWorkbench;
class QnWorkbenchContext;
class QnWorkbenchSynchronizer;
class QnWorkbenchLayoutSnapshotManager;
class QnWorkbenchActionHandler;
class QnActionManager;
class QnAction;
class QnCameraSettingsDialog;
class QnBusinessRulesDialog;
class QnCameraAdditionDialog;
class QnPopupCollectionWidget;
class QnWorkbenchNotificationsHandler;
class QnAdjustVideoDialog;
class QnSystemAdministrationDialog;
class QnGraphicsMessageBox;

// TODO: #Elric get rid of these processors here
namespace detail {
    class QnResourceStatusReplyProcessor: public QObject {
        Q_OBJECT
    public:
        QnResourceStatusReplyProcessor(QnWorkbenchActionHandler *handler, const QnVirtualCameraResourceList &resources);

    public slots:
        void at_replyReceived(int handle, ec2::ErrorCode errorCode, const QnResourceList& resources);

    private:
        QPointer<QnWorkbenchActionHandler> m_handler;
        QnVirtualCameraResourceList m_resources;
    };

    class QnResourceReplyProcessor: public QObject {
        Q_OBJECT
    public:
        QnResourceReplyProcessor(QObject *parent = NULL);

        int status() const {
            return m_status;
        }

        const QByteArray &errorString() const {
            return m_errorString;
        }

        const QnResourceList &resources() const {
            return m_resources;
        }

        int handle() const {
            return m_handle;
        }

    signals:
        void finished(int status, const QnResourceList &resources, int handle);

    public slots:
        void at_replyReceived(int status, const QnResourceList &resources, int handle);

    private:
        int m_handle;
        int m_status;
        QByteArray m_errorString;
        QnResourceList m_resources;
    };

} // namespace detail


// TODO: #Elric split this class into several handlers, group actions by handler. E.g. screen recording should definitely be spun off.
/**
 * This class implements logic for client actions.
 */
class QnWorkbenchActionHandler: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchActionHandler(QObject *parent = NULL);
    virtual ~QnWorkbenchActionHandler();

protected:
    ec2::AbstractECConnectionPtr connection2() const;

    struct AddToLayoutParams {
        bool usePosition;
        QPointF position;
        QRectF zoomWindow;
        QnUuid zoomUuid;
        qint64 time;
        QColor frameDistinctionColor;
        qreal rotation;
        ImageCorrectionParams contrastParams;
        QnItemDewarpingParams dewarpingParams;

        AddToLayoutParams():
            usePosition(false),
            position(QPointF()),
            zoomUuid(QnUuid()),
            time(-1),
            rotation(0)
        {}
    };

    void addToLayout(const QnLayoutResourcePtr &layout, const QnResourcePtr &resource, const AddToLayoutParams &params) const;
    void addToLayout(const QnLayoutResourcePtr &layout, const QnResourceList &resources, const AddToLayoutParams &params) const;
    void addToLayout(const QnLayoutResourcePtr &layout, const QList<QnMediaResourcePtr>& resources, const AddToLayoutParams &params) const;
    void addToLayout(const QnLayoutResourcePtr &layout, const QList<QString> &files, const AddToLayoutParams &params) const;

    QnResourceList addToResourcePool(const QString &file) const;
    QnResourceList addToResourcePool(const QList<QString> &files) const;

    void setLayoutAspectRatio(const QnLayoutResourcePtr &resource, double aspectRatio);

    void openResourcesInNewWindow(const QnResourceList &resources);

    void openNewWindow(const QStringList &args);

    void rotateItems(int degrees);

    void setResolutionMode(Qn::ResolutionMode resolutionMode);

    QnCameraSettingsDialog *cameraSettingsDialog() const;

    QnBusinessRulesDialog *businessRulesDialog() const;

    QnEventLogDialog *businessEventsLogDialog() const;

    QnCameraListDialog *cameraListDialog() const;

    QnCameraAdditionDialog *cameraAdditionDialog() const;

    QnSystemAdministrationDialog *systemAdministrationDialog() const;

    QnWorkbenchNotificationsHandler* notificationsHandler() const;

protected slots:
   
    void submitDelayedDrops();
    void submitInstantDrop();

protected slots:
    void at_context_userChanged(const QnUserResourcePtr &user);
    void at_workbench_layoutsChanged();
    void at_workbench_cellAspectRatioChanged();
    void at_workbench_cellSpacingChanged();
    void at_workbench_currentLayoutChanged();

    void at_mainMenuAction_triggered();
    void at_openCurrentUserLayoutMenuAction_triggered();

    void at_nextLayoutAction_triggered();
    void at_previousLayoutAction_triggered();
    void at_openLayoutsAction_triggered();
    void at_openNewTabAction_triggered();

    void at_openInLayoutAction_triggered();

    void at_openInCurrentLayoutAction_triggered();
    void at_openInNewLayoutAction_triggered();
    void at_openInNewWindowAction_triggered();

    void at_openLayoutsInNewWindowAction_triggered();
    void at_openCurrentLayoutInNewWindowAction_triggered();
    void at_openNewWindowAction_triggered();

    void at_moveCameraAction_triggered();
    void at_dropResourcesAction_triggered();
    void at_delayedDropResourcesAction_triggered();
    void at_instantDropResourcesAction_triggered();
    void at_dropResourcesIntoNewLayoutAction_triggered();
    void at_openFileAction_triggered();
    void at_openLayoutAction_triggered();
    void at_openFolderAction_triggered();
    void at_showcaseAction_triggered();
    void at_aboutAction_triggered();
    void at_businessEventsAction_triggered();
    void at_openBusinessRulesAction_triggered();
    void at_businessEventsLogAction_triggered();
    void at_openBusinessLogAction_triggered();
    void at_cameraListAction_triggered();
    void at_webClientAction_triggered();
    void at_systemAdministrationAction_triggered();
    void at_systemUpdateAction_triggered();
    void at_preferencesGeneralTabAction_triggered();
    void at_preferencesLicensesTabAction_triggered();
    void at_preferencesSmtpTabAction_triggered();
    void at_preferencesNotificationTabAction_triggered();
    void at_userSettingsAction_triggered();
    void at_cameraSettingsAction_triggered();
    void at_pictureSettingsAction_triggered();
    void at_cameraIssuesAction_triggered();
    void at_cameraBusinessRulesAction_triggered();
    void at_cameraDiagnosticsAction_triggered();
    void at_layoutSettingsAction_triggered();
    void at_currentLayoutSettingsAction_triggered();
    void at_serverAddCameraManuallyAction_triggered();
    void at_serverSettingsAction_triggered();
    void at_serverLogsAction_triggered();
    void at_serverIssuesAction_triggered();
    void at_pingAction_triggered();
    void at_thumbnailsSearchAction_triggered();

    void at_openInFolderAction_triggered();
    void at_deleteFromDiskAction_triggered();
    void at_removeLayoutItemAction_triggered();
    void at_renameAction_triggered();
    void at_removeFromServerAction_triggered();

    void at_newUserAction_triggered();

    void at_adjustVideoAction_triggered();
    void at_exitAction_triggered();
    void at_beforeExitAction_triggered();

    void at_setCurrentLayoutAspectRatio4x3Action_triggered();
    void at_setCurrentLayoutAspectRatio16x9Action_triggered();
    void at_setCurrentLayoutItemSpacing0Action_triggered();
    void at_setCurrentLayoutItemSpacing10Action_triggered();
    void at_setCurrentLayoutItemSpacing20Action_triggered();
    void at_setCurrentLayoutItemSpacing30Action_triggered();

    void at_createZoomWindowAction_triggered();

    void at_rotate0Action_triggered();
    void at_rotate90Action_triggered();
    void at_rotate180Action_triggered();
    void at_rotate270Action_triggered();

    void at_radassAutoAction_triggered();
    void at_radassLowAction_triggered();
    void at_radassHighAction_triggered();

    void at_setAsBackgroundAction_triggered();
    void at_backgroundImageStored(const QString &filename, bool success);

    void at_resources_saved( int handle, ec2::ErrorCode errorCode, const QnResourceList& resources );
    void at_resources_properties_saved( int handle, ec2::ErrorCode errorCode );
    void at_resource_deleted( int handle, ec2::ErrorCode errorCode );
    void at_resources_statusSaved(ec2::ErrorCode errorCode, const QnResourceList &resources);

    void at_panicWatcher_panicModeChanged();
    void at_scheduleWatcher_scheduleEnabledChanged();
    void at_togglePanicModeAction_toggled(bool checked);
    void at_updateWatcher_availableUpdateChanged();
    void at_layoutCountWatcher_layoutCountChanged();

    void at_toggleTourAction_toggled(bool checked);
    void at_tourTimer_timeout();
    void at_workbench_itemChanged(Qn::ItemRole role);

    void at_whatsThisAction_triggered();

    void at_escapeHotkeyAction_triggered();

    void at_clearCacheAction_triggered();

    void at_messageBoxAction_triggered();

    void at_browseUrlAction_triggered();

    void at_versionMismatchMessageAction_triggered();

    void at_betaVersionMessageAction_triggered();

    void at_queueAppRestartAction_triggered();
    void at_selectTimeServerAction_triggered();
    void at_cameraListChecked(int status, const QnCameraListReply& reply, int handle);
private:
    void notifyAboutUpdate();
    void checkVersionMismatches();

    void openLayoutSettingsDialog(const QnLayoutResourcePtr &layout);

    QnAdjustVideoDialog* adjustVideoDialog();

    /** Check if resource can be safely renamed to the new name. */
    bool validateResourceName(const QnResourcePtr &resource, const QString &newName) const;

    void deleteDialogs();
private:
    friend class detail::QnResourceStatusReplyProcessor;

    QPointer<QWidget> m_widget;
    QPointer<QMenu> m_mainMenu;
    QPointer<QMenu> m_currentUserLayoutsMenu;

    QPointer<QnCameraSettingsDialog> m_cameraSettingsDialog;
    QPointer<QnBusinessRulesDialog> m_businessRulesDialog;
    QPointer<QnEventLogDialog> m_businessEventsLogDialog;
    QPointer<QnCameraListDialog> m_cameraListDialog;
    QPointer<QnCameraAdditionDialog> m_cameraAdditionDialog;
    QPointer<QnAdjustVideoDialog> m_adjustVideoDialog;
    QPointer<QnSystemAdministrationDialog> m_systemAdministrationDialog;

    bool m_delayedDropGuard;
    /** List of serialized resources that are to be dropped on the scene once
     * the user logs in. */
    QList<QnMimeData> m_delayedDrops;
    QList<QnMimeData> m_instantDrops;

    QQueue<QnMediaResourcePtr> m_layoutExportResources;
    QString m_layoutFileName;
    QnTimePeriod m_exportPeriod;
    QnLayoutResourcePtr m_exportLayout;
    QnStorageResourcePtr m_exportStorage;

    QTimer *m_tourTimer;
    struct CameraMovingInfo 
    {
        CameraMovingInfo() {}
        CameraMovingInfo(const QnVirtualCameraResourceList& cameras, const QnResourcePtr& dstServer): cameras(cameras), dstServer(dstServer) {}
        QnVirtualCameraResourceList cameras;
        QnResourcePtr dstServer;
    };
    QMap<int, CameraMovingInfo> m_awaitingMoveCameras;
};

#endif // QN_WORKBENCH_ACTION_HANDLER_H
