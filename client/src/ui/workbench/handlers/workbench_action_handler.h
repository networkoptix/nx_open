#ifndef QN_WORKBENCH_ACTION_HANDLER_H
#define QN_WORKBENCH_ACTION_HANDLER_H

#include <QtCore/QBuffer>
#include <QtCore/QObject>


#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>
#include <ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>
#include <client/client_globals.h>
#include <client/client_settings.h>
#include "ui/dialogs/event_log_dialog.h"
#include "ui/dialogs/camera_list_dialog.h"

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
class QnLoginDialog;
class QnVideoCamera;
class QnPopupCollectionWidget;
class QnWorkbenchNotificationsHandler;
class QnAdjustVideoDialog;


namespace detail {
    class QnResourceStatusReplyProcessor: public QObject {
        Q_OBJECT
    public:
        QnResourceStatusReplyProcessor(QnWorkbenchActionHandler *handler, const QnVirtualCameraResourceList &resources, const QList<int> &oldDisabledFlags);

    public slots:
        void at_replyReceived(int status, const QnResourceList &resources, int handle);

    private:
        QPointer<QnWorkbenchActionHandler> m_handler;
        QnVirtualCameraResourceList m_resources;
        QList<int> m_oldDisabledFlags;
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
    QnAppServerConnectionPtr connection() const;

    QString newLayoutName(const QnUserResourcePtr &user, const QString &baseName = tr("New layout")) const;
    bool canAutoDelete(const QnResourcePtr &resource) const;

    struct AddToLayoutParams {
        bool usePosition;
        QPointF position;
        QRectF zoomWindow;
        QUuid zoomUuid;
        qint64 time;
        QColor frameColor;
        qreal rotation;
        ImageCorrectionParams contrastParams;
        DewarpingParams dewarpingParams;

        AddToLayoutParams():
            usePosition(false),
            position(QPointF()),
            zoomUuid(QUuid()),
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

    void closeLayouts(const QnLayoutResourceList &resources, const QnLayoutResourceList &rollbackResources, const QnLayoutResourceList &saveResources, QObject *object, const char *slot);
    bool closeLayouts(const QnLayoutResourceList &resources, bool waitForReply = false);
    bool closeLayouts(const QnWorkbenchLayoutList &layouts, bool waitForReply = false);
    bool closeAllLayouts(bool waitForReply = false);

    void setLayoutAspectRatio(const QnLayoutResourcePtr &resource, double aspectRatio);

    void openResourcesInNewWindow(const QnResourceList &resources);

    void openNewWindow(const QStringList &args);

    /**
     * Save modified camera settings to resources.
     * \param checkControls - if set then additional check will occur.
     * If user modified some of control elements but did not apply changes he will be asked to fix it.
     * \see Feature #1195
     */
    void saveCameraSettingsFromDialog(bool checkControls = false);

    void rotateItems(int degrees);

    void setResolutionMode(Qn::ResolutionMode resolutionMode);

    QnCameraSettingsDialog *cameraSettingsDialog() const;

    QnBusinessRulesDialog *businessRulesDialog() const;

    QnEventLogDialog *businessEventsLogDialog() const;

    QnCameraListDialog *cameraListDialog() const;

    QnCameraAdditionDialog *cameraAdditionDialog() const;

    QnLoginDialog *loginDialog() const;

    QnWorkbenchNotificationsHandler* notificationsHandler() const;

protected slots:
    void updateCameraSettingsFromSelection();
    void updateCameraSettingsEditibility();
    void submitDelayedDrops();
    void submitInstantDrop(QnMimeData &data);

protected slots:
    void at_context_userChanged(const QnUserResourcePtr &user);
    void at_workbench_layoutsChanged();
    void at_workbench_cellAspectRatioChanged();
    void at_workbench_cellSpacingChanged();
    void at_workbench_currentLayoutChanged();

    void at_messageProcessor_connectionClosed();
    void at_messageProcessor_connectionOpened();
    void at_eventManager_actionReceived(const QnAbstractBusinessActionPtr& businessAction);

    void at_mainMenuAction_triggered();
    void at_openCurrentUserLayoutMenuAction_triggered();

    void at_debugIncrementCounterAction_triggered();
    void at_debugDecrementCounterAction_triggered();
    void at_debugShowResourcePoolAction_triggered();
    void at_debugCalibratePtzAction_triggered();

    void at_nextLayoutAction_triggered();
    void at_previousLayoutAction_triggered();
    void at_openLayoutsAction_triggered();
    void at_openNewTabAction_triggered();
    void at_openInLayoutAction_triggered();
    void at_openInCurrentLayoutAction_triggered();
    void at_openInNewLayoutAction_triggered();
    void at_openLayoutsInNewWindowAction_triggered();
    void at_openCurrentLayoutInNewWindowAction_triggered();
    void at_openInNewWindowAction_triggered();
    void at_openNewWindowAction_triggered();
    void at_saveLayoutAction_triggered(const QnLayoutResourcePtr &layout);
    void at_saveLayoutAction_triggered();
    void at_saveCurrentLayoutAction_triggered();
    void at_saveLayoutAsAction_triggered(const QnLayoutResourcePtr &layout, const QnUserResourcePtr &user);
    void at_saveLayoutAsAction_triggered();
    void at_saveLayoutForCurrentUserAsAction_triggered();
    void at_saveCurrentLayoutAsAction_triggered();
    void at_closeLayoutAction_triggered();
    void at_closeAllButThisLayoutAction_triggered();

    void at_moveCameraAction_triggered();
    void at_dropResourcesAction_triggered();
    void at_delayedDropResourcesAction_triggered();
    void at_instantDropResourcesAction_triggered();
    void at_dropResourcesIntoNewLayoutAction_triggered();
    void at_openFileAction_triggered();
    void at_openLayoutAction_triggered();
    void at_openFolderAction_triggered();
    void at_checkForUpdatesAction_triggered();
    void at_showcaseAction_triggered();
    void at_aboutAction_triggered();
    void at_businessEventsAction_triggered();
    void at_openBusinessRulesAction_triggered();
    void at_businessEventsLogAction_triggered();
    void at_openBusinessLogAction_triggered();
    void at_cameraListAction_triggered();
    void at_webClientAction_triggered();
    void at_preferencesGeneralTabAction_triggered();
    void at_preferencesLicensesTabAction_triggered();
    void at_preferencesServerTabAction_triggered();
    void at_preferencesNotificationTabAction_triggered();
    void at_connectToServerAction_triggered();
    void at_reconnectAction_triggered();
    void at_disconnectAction_triggered();
    void at_userSettingsAction_triggered();
    void at_cameraSettingsAction_triggered();
    void at_cameraIssuesAction_triggered();
    void at_cameraBusinessRulesAction_triggered();
    void at_cameraDiagnosticsAction_triggered();
    void at_layoutSettingsAction_triggered();
    void at_currentLayoutSettingsAction_triggered();
    void at_clearCameraSettingsAction_triggered();
    void at_cameraSettingsDialog_buttonClicked(QDialogButtonBox::StandardButton button);
    void at_cameraSettingsDialog_scheduleExported(const QnVirtualCameraResourceList &cameras);
    void at_cameraSettingsDialog_rejected();
    void at_cameraSettingsDialog_advancedSettingChanged();
    void at_cameraSettingsDialog_cameraOpenRequested();
    void at_cameraSettingsDialog_cameraIssuesRequested();
    void at_cameraSettingsDialog_cameraRulesRequested();
    void at_selectionChangeAction_triggered();
    void at_serverAddCameraManuallyAction_triggered();
    void at_serverSettingsAction_triggered();
    void at_serverLogsAction_triggered();
    void at_serverIssuesAction_triggered();
    void at_pingAction_triggered();
    void at_youtubeUploadAction_triggered();
    void at_thumbnailsSearchAction_triggered();

    void at_openInFolderAction_triggered();
    void at_deleteFromDiskAction_triggered();
    void at_removeLayoutItemAction_triggered();
    void at_renameAction_triggered();
    void at_removeFromServerAction_triggered();

    void at_newUserAction_triggered();
    void at_newUserLayoutAction_triggered();

    void at_adjustVideoAction_triggered();
    void at_exitAction_triggered();

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

    void at_ptzSavePresetAction_triggered();
    void at_ptzGoToPresetAction_triggered();
    void at_ptzManagePresetsAction_triggered();

    void at_setAsBackgroundAction_triggered();
    void at_backgroundImageStored(const QString &filename, bool success);

    void at_exportTimeSelectionAction_triggered();
    void at_exportLayoutAction_triggered();
    void at_camera_exportFinished(QString fileName);
    void at_camera_exportFailed(QString errorMessage);

    void at_resources_saved(int status, const QnResourceList &resources, int handle);
    void at_resource_deleted(const QnHTTPRawResponse& resource, int handle);
    void at_resources_statusSaved(int status, const QnResourceList &resources, const QList<int> &oldDisabledFlags);

    void at_panicWatcher_panicModeChanged();
    void at_scheduleWatcher_scheduleEnabledChanged();
    void at_togglePanicModeAction_toggled(bool checked);
    void at_updateWatcher_availableUpdateChanged();
    void at_layoutCountWatcher_layoutCountChanged();

    void at_toggleTourAction_toggled(bool checked);
    void at_toggleTourModeHotkeyAction_triggered();
    void at_tourTimer_timeout();
    void at_workbench_itemChanged(Qn::ItemRole role);

    void at_layoutCamera_exportFinished(QString fileName);
    void at_layoutCamera_exportFinished2();
    void at_layout_exportFinished();
    void at_layoutCamera_exportFailed(QString errorMessage);

    void at_camera_settings_saved(int httpStatusCode, const QList<QPair<QString, bool> >& operationResult);

    void at_cancelExport();

    void at_whatsThisAction_triggered();

    void at_escapeHotkeyAction_triggered();

    void at_clearCacheAction_triggered();

    void at_messageBoxAction_triggered();

    void at_browseUrlAction_triggered();

    void at_versionMismatchMessageAction_triggered();
    void at_versionMismatchWatcher_mismatchDataChanged();

private:
    enum LayoutExportMode {LayoutExport_LocalSave, LayoutExport_LocalSaveAs, LayoutExport_Export};

    void saveAdvancedCameraSettingsAsync(QnVirtualCameraResourceList cameras);
    void saveLayoutToLocalFile(const QnTimePeriod& exportPeriod, QnLayoutResourcePtr layout, const QString& layoutFileName, LayoutExportMode mode, bool exportReadOnly);
    bool doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod, QnLayoutResourcePtr layout, LayoutExportMode mode);
#ifdef Q_OS_WIN
    QString binaryFilterName() const;
#endif
    bool validateItemTypes(QnLayoutResourcePtr layout); // used for export local layouts. Disable cameras and local items for same layout
    void removeLayoutFromPool(QnLayoutResourcePtr existingLayout);
    void notifyAboutUpdate(bool alwaysNotify);

    /**
     * @brief alreadyExistingLayouts    Check if layouts with same name already exist.
     * @param name                      Suggested new name.
     * @param user                      User that will own the layout.
     * @param layout                    Layout that we want to rename (if any).
     * @return                          List of existing layouts with same name.
     */
    QnLayoutResourceList alreadyExistingLayouts(const QString &name, const QnUserResourcePtr &user, const QnLayoutResourcePtr &layout = QnLayoutResourcePtr());

    /**
     * @brief askOverrideLayout     Show messagebox asking user if he really wants to override existsing layout.
     * @param buttons               Messagebox buttons.
     * @param defaultButton         Default button.
     * @return                      Selected button.
     */
    QMessageBox::StandardButton askOverrideLayout(QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton);

    bool canRemoveLayouts(const QnLayoutResourceList &layouts);

    void removeLayouts(const QnLayoutResourceList &layouts);

    void openLayoutSettingsDialog(const QnLayoutResourcePtr &layout);

    QnAdjustVideoDialog* adjustVideoDialog();

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
    QPointer<QnLoginDialog> m_loginDialog;
    QPointer<QnAdjustVideoDialog> m_adjustVideoDialog;


    /** Whether the set of selected resources was changed and settings
     * dialog is waiting to be updated. */
    bool m_selectionUpdatePending;

    /** Scope of the last selection change. */
    Qn::ActionScope m_selectionScope;

    /** List of serialized resources that are to be dropped on the scene once
     * the user logs in. */
    QList<QnMimeData> m_delayedDrops;

    QnVideoCamera* m_layoutExportCamera;
    QnVideoCamera* m_exportedCamera;
    QQueue<QnMediaResourcePtr> m_layoutExportResources;
    QString m_layoutFileName;
    QnTimePeriod m_exportPeriod;
    QPointer<QnProgressDialog> m_exportProgressDialog;
    QnLayoutResourcePtr m_exportLayout;
    QnStorageResourcePtr m_exportStorage;
    QSharedPointer<QBuffer> m_motionFileBuffer[CL_MAX_CHANNELS];
    QnMediaResourcePtr m_exportedMediaRes;
    //QString m_layoutExportMessage;
    LayoutExportMode m_layoutExportMode;
    int m_exportRetryCount; // anitvirus sometimes block exe file. workaround
    QString m_exportTmpFileName;

    QTimer *m_tourTimer;
};

#endif // QN_WORKBENCH_ACTION_HANDLER_H
