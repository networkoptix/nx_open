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

protected slots:
    void at_workbench_aboutToBeDestroyed();
    void at_synchronizer_destroyed();

    void showOpenFileDialog();
    void showAboutDialog();
    void showPreferencesDialog();
    void showAuthenticationDialog();

    void addNewLayout();
    void openNewLayout();

    void at_closeLayoutAction_triggered();
    void at_cameraSettingsAction_triggered();
    void at_multipleCamerasSettingsAction_triggered();
    void at_serverSettingsAction_triggered();
    void at_youtubeUploadAction_triggered();
    void at_editTagsAction_triggered();
    void at_openInFolderAction_triggered();
    void at_removeLayoutItemAction_triggered();
    void at_removeFromServerAction_triggered();

    void at_layout_saved(int status, const QByteArray &errorString, const QnLayoutResourcePtr &resource);
    void at_resource_deleted(int status, const QByteArray &data, const QByteArray &errorString, int handle);

private:
    QnWorkbench *m_workbench;
    QnWorkbenchSynchronizer *m_synchronizer;
    QWeakPointer<QWidget> m_widget;
    QnAppServerConnectionPtr m_connection;
};

#endif // QN_WORKBENCH_ACTION_HANDLER_H
