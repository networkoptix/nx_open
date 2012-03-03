#ifndef QN_WORKBENCH_ACTION_HANDLER_H
#define QN_WORKBENCH_ACTION_HANDLER_H

#include <QObject>
#include <QWeakPointer>
#include <api/AppServerConnection.h>
#include <ui/actions/actions.h>

class QAction;
class QMenu;

class QnResourcePool;
class QnWorkbench;
class QnWorkbenchContext;
class QnWorkbenchSynchronizer;
class QnWorkbenchLayoutSnapshotManager;
class QnActionManager;
class QnAction;

class QnWorkbenchActionHandler: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchActionHandler(QObject *parent = NULL);

    virtual ~QnWorkbenchActionHandler();

    QnWorkbenchContext *context() const {
        return m_context;
    }

    QnWorkbench *workbench() const;

    QnWorkbenchSynchronizer *synchronizer() const;

    QnWorkbenchLayoutSnapshotManager *snapshotManager() const;

    QnActionManager *menu() const;

    QAction *action(const Qn::ActionId id);

    QnResourcePool *resourcePool() const;

    void setContext(QnWorkbenchContext *context);

    QWidget *widget() const {
        return m_widget.data();
    }

    void setWidget(QWidget *widget) {
        m_widget = widget;
    }

protected:
    void initialize();
    void deinitialize();

    QString newLayoutName() const;
    bool canAutoDelete(const QnResourcePtr &resource) const;
    void addToWorkbench(const QnResourcePtr &resource, bool usePosition, const QPointF &position = QPointF()) const;
    void addToWorkbench(const QnResourceList &resources, bool usePosition, const QPointF &position = QPointF()) const;
    void addToWorkbench(const QnMediaResourceList &resources, bool usePosition, const QPointF &position = QPointF()) const;
    void addToWorkbench(const QList<QString> &files, bool usePosition, const QPointF &position = QPointF()) const;

protected slots:
    void at_context_aboutToBeDestroyed();
    void at_context_userChanged(const QnUserResourcePtr &user);
    void at_workbench_layoutsChanged();

    void at_mainMenuAction_triggered();

    void at_openLayoutAction_triggered();
    void at_openNewLayoutAction_triggered();
    void at_saveLayoutAction_triggered(const QnLayoutResourcePtr &layout);
    void at_saveLayoutAction_triggered();
    void at_saveCurrentLayoutAction_triggered();
    void at_saveLayoutAsAction_triggered(const QnLayoutResourcePtr &layout);
    void at_saveLayoutAsAction_triggered();
    void at_saveCurrentLayoutAsAction_triggered();
    void at_closeLayoutAction_triggered();
    
    void at_resourceDropAction_triggered();
    void at_openFileAction_triggered();
    void at_openFolderAction_triggered();
    void at_aboutAction_triggered();
    void at_systemSettingsAction_triggered();
    void at_connectionSettingsAction_triggered();
    void at_cameraSettingsAction_triggered();
    void at_multipleCamerasSettingsAction_triggered();
    void at_serverSettingsAction_triggered();
    void at_youtubeUploadAction_triggered();
    void at_editTagsAction_triggered();

    void at_openInFolderAction_triggered();
    void at_deleteFromDiskAction_triggered();
    void at_removeLayoutItemAction_triggered();
    void at_renameLayoutAction_triggered();
    void at_removeFromServerAction_triggered();

    void at_newUserAction_triggered();
    void at_newLayoutAction_triggered();

    void at_layout_saved(int status, const QByteArray &errorString, const QnLayoutResourcePtr &resource);
    void at_user_saved(int status, const QByteArray &errorString, const QnResourceList &resources, int handle);
    void at_resource_deleted(int status, const QByteArray &data, const QByteArray &errorString, int handle);

private:
    QnWorkbenchContext *m_context;
    QWeakPointer<QWidget> m_widget;
    QnAppServerConnectionPtr m_connection;

    QScopedPointer<QMenu> m_mainMenu;
};

#endif // QN_WORKBENCH_ACTION_HANDLER_H
