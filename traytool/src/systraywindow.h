#ifndef WINDOW_H
#define WINDOW_H

#include <QtWidgets/QDialog>
#include <QtCore/QString>
#include <QtCore/QStringListModel>
#include <QtCore/QRunnable>
#include <QtCore/QTimer>
#include <QtCore/QTime>
#include <QtCore/QQueue>
#include <QtCore/QSettings>
#include <QtWidgets/QSystemTrayIcon>

class FoundEnterpriseControllersModel;

class QAbstractButton;
class QAction;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSpinBox;
class QTextEdit;

namespace Ui {
    class SettingsDialog;
}

bool MyIsUserAnAdmin();

class QnElevationChecker : public QObject
{
    Q_OBJECT

public:
    QnElevationChecker(QObject* parent, QString actionName, QObject* target, const char* slot);

signals:
    void elevationCheckPassed();

private slots:
    void triggered();

private:
    QString m_actionName;
    QObject* m_target;
    const char* m_slot;
    bool m_rightWarnShowed;
};

class StopServiceAsyncTask: public QObject, public QRunnable
{
    Q_OBJECT
public:
    StopServiceAsyncTask(SC_HANDLE handle): m_handle(handle) {}
    void run()
    {
        SERVICE_STATUS serviceStatus;
        ControlService(m_handle, SERVICE_CONTROL_STOP, &serviceStatus);
        emit finished();
    }
signals:
    void finished();
private:
    SC_HANDLE m_handle;
};

class GetServiceInfoAsyncTask: public QObject, public QRunnable
{
    Q_OBJECT
public:
    GetServiceInfoAsyncTask(SC_HANDLE handle): m_handle(handle) {}
    void run()
    {
        SERVICE_STATUS serviceStatus;
        if (QueryServiceStatus(m_handle, &serviceStatus))
            emit finished((quint64)serviceStatus.dwCurrentState);
    }
signals:
    void finished(quint64);
private:
    SC_HANDLE m_handle;
};

class QnSystrayWindow : public QDialog
{
    Q_OBJECT
    typedef QDialog base_type;

public:
    QnSystrayWindow();

    virtual ~QnSystrayWindow();
    void executeAction(const QString &cmd);

    void showMessage(const QString &title, const QString &msg, QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);

protected:
    virtual void closeEvent(QCloseEvent *event) override;

private slots:
    void handleMessage(const QString& message);
    
    void onDelayedMessage();

    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    

    void at_mediaServerStartAction();
    void at_mediaServerStopAction();
    void at_mediaServerWebAction();
    void onShowMediaServerLogAction();

    void findServiceInfo();
    void updateServiceInfo();
    void mediaServerInfoUpdated(quint64 status);

private:
    QAction* actionByName(const QString& name);
    QString nameByAction(QAction* action);

    void connectElevatedAction(QAction* source, const char* signal, QObject* target, const char* slot);
    void updateServiceInfoInternal(SC_HANDLE handle, DWORD status, QAction* startAction, QAction* stopAction);

    void createActions();
    void updateActions();
    void createTrayIcon();

    bool isServerInstalled() const;
private:
    QSettings m_mediaServerSettings;

    QAction *m_quitAction;

    QSystemTrayIcon *m_trayIcon;

    SC_HANDLE m_scManager;
    SC_HANDLE m_mediaServerHandle;

    QAction *m_showMediaServerLogAction;    
    QAction* m_mediaServerStartAction;
    QAction* m_mediaServerStopAction;
    QAction* m_mediaServerWebAction;
    bool m_firstTimeToolTipError;

    bool m_needStartMediaServer;
    int m_prevMediaServerStatus;

    QList<QAction *> m_actions;
    QTime m_lastMessageTimer;

    struct DelayedMessage {
        QString title;
        QString msg;
        QSystemTrayIcon::MessageIcon icon;
        DelayedMessage(const QString &title, const QString &msg, QSystemTrayIcon::MessageIcon icon);
    };

    QQueue<DelayedMessage> m_delayedMessages;
    QString m_mediaServerServiceName;
};

#endif
