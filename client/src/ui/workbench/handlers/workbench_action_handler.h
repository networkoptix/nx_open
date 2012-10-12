#ifndef QN_WORKBENCH_ACTION_HANDLER_H
#define QN_WORKBENCH_ACTION_HANDLER_H

#include <QtCore/QObject>
#include <QtCore/QWeakPointer>

#include <QtGui/QDialogButtonBox>

#include <api/app_server_connection.h>
#include <ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_globals.h>
#include <utils/settings.h>

class QAction;
class QMenu;
class QProgressDialog;

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
class QnVideoCamera;

// TODO: move out.
struct QnThumbnailsSearchState {
    QnThumbnailsSearchState(): step(0) {}
    QnThumbnailsSearchState(const QnTimePeriod &period, qint64 step): period(period), step(step) {}

    QnTimePeriod period;
    qint64 step;
};
Q_DECLARE_METATYPE(QnThumbnailsSearchState)


namespace detail {
    class QnResourceStatusReplyProcessor: public QObject {
        Q_OBJECT
    public:
        QnResourceStatusReplyProcessor(QnWorkbenchActionHandler *handler, const QnVirtualCameraResourceList &resources, const QList<int> &oldDisabledFlags);

    public slots:
        void at_replyReceived(int status, const QByteArray &errorString, const QnResourceList &resources, int handle);

    private:
        QWeakPointer<QnWorkbenchActionHandler> m_handler;
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
        void finished(int status, const QByteArray &errorString, const QnResourceList &resources, int handle);

    public slots:
        void at_replyReceived(int status, const QByteArray &errorString, const QnResourceList &resources, int handle);

    private:
        int m_handle;
        int m_status;
        QByteArray m_errorString;
        QnResourceList m_resources;
    };

    class QnConnectReplyProcessor: public QObject {
        Q_OBJECT
    public:
        QnConnectReplyProcessor(QObject *parent = NULL);

        int status() const {
            return m_status;
        }

        const QByteArray &errorString() const {
            return m_errorString;
        }

        const QnConnectInfoPtr &info() const {
            return m_connectInfo;
        }

        int handle() const {
            return m_handle;
        }

    signals:
        void finished(int status, const QByteArray &errorString, const QnConnectInfoPtr &connectInfo, int handle);

    public slots:
        void at_replyReceived(int status, const QByteArray &errorString, const QnConnectInfoPtr &connectInfo, int handle);

    private:
        int m_handle;
        int m_status;
        QByteArray m_errorString;
        QnConnectInfoPtr m_connectInfo;
    };

} // namespace detail


// TODO: split this class into several handlers, group actions by handler. E.g. screen recording should definitely be spun off.
/**
 * This class implements logic for client actions.
 */
class QnWorkbenchActionHandler: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchActionHandler(QObject *parent = NULL);
    virtual ~QnWorkbenchActionHandler();

    /**
     * \returns                         Widget that this action handler operates on.
     */
    QWidget *widget() const;

    /**
     * \param widget                    Widget for this action handler to operate on.
     *                                  All dialogs and message boxes will be displayed
     *                                  to the user relative to this widget.
     */
    void setWidget(QWidget *widget) {
        m_widget = widget;
    }

protected:
    QnAppServerConnectionPtr connection() const;

    QString newLayoutName() const;
    bool canAutoDelete(const QnResourcePtr &resource) const;
    void addToLayout(const QnLayoutResourcePtr &layout, const QnResourcePtr &resource, bool usePosition, const QPointF &position = QPointF()) const;
    void addToLayout(const QnLayoutResourcePtr &layout, const QnResourceList &resources, bool usePosition, const QPointF &position = QPointF()) const;
    void addToLayout(const QnLayoutResourcePtr &layout, const QnMediaResourceList &resources, bool usePosition, const QPointF &position = QPointF()) const;
    void addToLayout(const QnLayoutResourcePtr &layout, const QList<QString> &files, bool usePosition, const QPointF &position = QPointF()) const;
    
    QnResourceList addToResourcePool(const QString &file) const;
    QnResourceList addToResourcePool(const QList<QString> &files) const;
    
    void closeLayouts(const QnLayoutResourceList &resources, const QnLayoutResourceList &rollbackResources, const QnLayoutResourceList &saveResources, QObject *object, const char *slot);
    bool closeLayouts(const QnLayoutResourceList &resources, bool waitForReply = false);
    bool closeLayouts(const QnWorkbenchLayoutList &layouts, bool waitForReply = false);
    bool closeAllLayouts(bool waitForReply = false);

    void setLayoutAspectRatio(const QnLayoutResourcePtr &resource, double aspectRatio);

    void openNewWindow(const QStringList &args);

    /**
     * Save modified camera settings to resources.
     * \param checkControls - if set then additional check will occur.
     * If user modified some of control elements but did not apply changes he will be asked to fix it.
     * \see Feature #1195
     */
    void saveCameraSettingsFromDialog(bool checkControls = false);

    void rotateItems(int degrees);
    
    QnCameraSettingsDialog *cameraSettingsDialog() const {
        return m_cameraSettingsDialog.data();
    }

protected slots:
    void updateCameraSettingsFromSelection();
    void updateCameraSettingsEditibility();
    void submitDelayedDrops();

protected slots:
    void at_context_userChanged(const QnUserResourcePtr &user);
    void at_workbench_layoutsChanged();
    void at_workbench_cellAspectRatioChanged();
    void at_workbench_cellSpacingChanged();

    void at_eventManager_connectionClosed();
    void at_eventManager_connectionOpened();

    void at_mainMenuAction_triggered();

    void at_incrementDebugCounterAction_triggered();
    void at_decrementDebugCounterAction_triggered();

    void at_nextLayoutAction_triggered();
    void at_previousLayoutAction_triggered();
    void at_openLayoutsAction_triggered();
    void at_openNewWindowLayoutsAction_triggered();
    void at_openNewTabAction_triggered();
    void at_openInLayoutAction_triggered();
    void at_openInCurrentLayoutAction_triggered();
    void at_openInNewLayoutAction_triggered();
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
    void at_dropResourcesIntoNewLayoutAction_triggered();
    void at_openFileAction_triggered();
    void at_openLayoutAction_triggered();
    void at_openFolderAction_triggered();
    void at_aboutAction_triggered();
    void at_systemSettingsAction_triggered();
    void at_getMoreLicensesAction_triggered();
    void at_connectToServerAction_triggered();
    void at_reconnectAction_triggered();
    void at_userSettingsAction_triggered();
    void at_cameraSettingsAction_triggered();
    void at_clearCameraSettingsAction_triggered();
    void at_cameraSettingsDialog_buttonClicked(QDialogButtonBox::StandardButton button);
    void at_cameraSettingsDialog_rejected();
    void at_cameraSettingsAdvanced_changed();
    void at_selectionChangeAction_triggered();
    void at_serverAddCameraManuallyAction_triggered();
    void at_serverSettingsAction_triggered();
    void at_youtubeUploadAction_triggered();
    void at_editTagsAction_triggered();
    void at_thumbnailsSearchAction_triggered();

    void at_openInFolderAction_triggered();
    void at_deleteFromDiskAction_triggered();
    void at_removeLayoutItemAction_triggered();
    void at_renameAction_triggered();
    void at_removeFromServerAction_triggered();
    
    void at_newUserAction_triggered();
    void at_newUserLayoutAction_triggered();

    void at_takeScreenshotAction_triggered();
    void at_exitAction_triggered();

    void at_setCurrentLayoutAspectRatio4x3Action_triggered();
    void at_setCurrentLayoutAspectRatio16x9Action_triggered();
    void at_setCurrentLayoutItemSpacing0Action_triggered();
    void at_setCurrentLayoutItemSpacing10Action_triggered();
    void at_setCurrentLayoutItemSpacing20Action_triggered();
    void at_setCurrentLayoutItemSpacing30Action_triggered();

    void at_rotate0Action_triggered();
    void at_rotate90Action_triggered();
    void at_rotate180Action_triggered();
    void at_rotate270Action_triggered();

    void at_exportTimeSelectionAction_triggered();
    void at_exportLayoutAction_triggered();
    void at_camera_exportFinished(QString fileName);
    void at_camera_exportFailed(QString errorMessage);

    void at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle);
    void at_resource_deleted(int status, const QByteArray &data, const QByteArray &errorString, int handle);
    void at_resources_statusSaved(int status, const QByteArray &errorString, const QnResourceList &resources, const QList<int> &oldDisabledFlags);

    void at_panicWatcher_panicModeChanged();
    void at_scheduleWatcher_scheduleEnabledChanged();
    void at_togglePanicModeAction_toggled(bool checked);
    void at_updateWatcher_availableUpdateChanged();

    void at_toggleTourAction_toggled(bool checked);
    void at_tourTimer_timeout();
    void at_workbench_itemChanged(Qn::ItemRole role);

    void at_layoutCamera_exportFinished(QString fileName);
    void at_layout_exportFinished();
    void at_cameraCamera_exportFailed(QString errorMessage);

    void at_camera_settings_saved(int httpStatusCode, const QList<QPair<QString, bool> >& operationResult);

private:
    enum LayoutExportMode {LayoutExport_LocalSave, LayoutExport_LocalSaveAs, LayoutExport_Export};

    void saveAdvancedCameraSettingsAsync(QnVirtualCameraResourceList cameras);
    void saveLayoutToLocalFile(const QnTimePeriod& exportPeriod, QnLayoutResourcePtr layout, const QString& layoutFileName, LayoutExportMode mode);
    // void updateStoredConnections(QnConnectionData connectionData);
    bool doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod, QnLayoutResourcePtr layout, LayoutExportMode mode);
    QString binaryFilterName() const;
    bool validateItemTypes(QnLayoutResourcePtr layout); // used for export local layouts. Disable cameras and local items for same layout

private:
    friend class detail::QnResourceStatusReplyProcessor;

    QWeakPointer<QWidget> m_widget;
    QWeakPointer<QMenu> m_mainMenu;
    
    QWeakPointer<QnCameraSettingsDialog> m_cameraSettingsDialog;

    /** Whether the set of selected resources was changed and settings
     * dialog is waiting to be updated. */
    bool m_selectionUpdatePending;

    /** Scope of the last selection change. */
    Qn::ActionScope m_selectionScope;

    /** List of serialized resources that are to be dropped on the scene once
     * the user logs in. */
    QList<QnMimeData> m_delayedDrops;

    QnVideoCamera* m_layoutExportCamera;
    QQueue<QnMediaResourcePtr> m_layoutExportResources;
    QString m_layoutFileName;
    QnTimePeriod m_exportPeriod;
    QWeakPointer<QProgressDialog> m_exportProgressDialog;
    QnStorageResourcePtr m_exportStorage;  
    QSharedPointer<QBuffer> m_motionFileBuffer[CL_MAX_CHANNELS];
    QnMediaResourcePtr m_exportedMediaRes;
    //QString m_layoutExportMessage;
    LayoutExportMode m_layoutExportMode;


    QTimer *m_tourTimer;
};

#endif // QN_WORKBENCH_ACTION_HANDLER_H
