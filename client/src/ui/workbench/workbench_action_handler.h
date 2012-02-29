#ifndef QN_WORKBENCH_ACTION_HANDLER_H
#define QN_WORKBENCH_ACTION_HANDLER_H

#include <QObject>
#include <QWeakPointer>
#include <api/AppServerConnection.h>

class QnWorkbench;
class QnWorkbenchSynchronizer;

class QnWorkbenchActionHandler: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchActionHandler(QObject *parent = NULL);

    virtual ~QnWorkbenchActionHandler();

    QnWorkbench *workbench() const {
        return m_workbench;
    }

    void setWorkbench(QnWorkbench *workbench);

    QnWorkbenchSynchronizer *synchronizer() const {
        return m_synchronizer;
    }

    void setSynchronizer(QnWorkbenchSynchronizer *synchronizer);

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
    void at_workbench_aboutToBeDestroyed();
    void at_workbench_layoutsChanged();
    void at_synchronizer_destroyed();

    void at_openLayoutAction_triggered();
    void at_openNewLayoutAction_triggered();
    void at_openSingleLayoutAction_triggered();
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
    QnWorkbench *m_workbench;
    QnWorkbenchSynchronizer *m_synchronizer;
    QWeakPointer<QWidget> m_widget;
    QnAppServerConnectionPtr m_connection;
};

#endif // QN_WORKBENCH_ACTION_HANDLER_H
