#ifndef WINDOW_H
#define WINDOW_H

#include <QDialog>
#include <QString>
#include <QStringListModel>
#include <QSystemTrayIcon>


class FoundEnterpriseControllersModel;

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
    class FindAppServerDialog;
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

public:
    QnSystrayWindow( FoundEnterpriseControllersModel* const foundEnterpriseControllersModel );

    virtual ~QnSystrayWindow();
    QSystemTrayIcon* getTrayIcon() const { return trayIcon; }
    void executeAction(QString cmd);
    void setVisible(bool visible);

protected:
    virtual void closeEvent(QCloseEvent *event) override;

private slots:
    void handleMessage(const QString& message);
    
    void onDelayedMessage();

    void setIcon(int index);
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void showMessage(const QString& message);
    void messageClicked();

    void at_mediaServerStartAction();
    void at_mediaServerStopAction();
    void at_appServerStartAction();
    void at_appServerStopAction();

    void findServiceInfo();
    void updateServiceInfo();
    void appServerInfoUpdated(quint64 status);
    void mediaServerInfoUpdated(quint64 status);

    void buttonClicked(QAbstractButton * button);
    void onSettingsAction();
    void onShowMediaServerLogAction();
    void onShowAppServerLogAction();

    void onTestButtonClicked();

    void onFindAppServerButtonClicked();
    void onAppServerUrlHistoryComboBoxCurrentChanged( int index );
    void onRadioButtonPublicIpChanged();

    void at_appServerPassword_textChanged(const QString &text);
private:
    QAction* actionByName(const QString& name);
    QString nameByAction(QAction* action);

    void connectElevatedAction(QAction* source, const char* signal, QObject* target, const char* slot);

    void createActions();
    void createTrayIcon();

    void updateServiceInfoInternal(SC_HANDLE service, DWORD status, const QString& serviceName, QAction* startAction, QAction* stopAction, QAction* logAction);
    bool validateData();
    void saveData();
    bool checkPortNum(int port, const QString& message);
    bool isPortFree(int port);
    QUrl getAppServerURL() const;
    void setAppServerURL(const QUrl& url);
    bool isAppServerParamChanged() const;
    bool isMediaServerParamChanged() const;

    QScopedPointer<Ui::SettingsDialog> ui;
    QScopedPointer<QDialog> m_findAppServerDialog;
    QScopedPointer<Ui::FindAppServerDialog> m_findAppServerDialogUI;

    QSettings m_settings;
    QSettings m_mServerSettings;
    QSettings m_appServerSettings;

    QAction *m_showMediaServerLogAction;
    QAction *m_showAppLogAction;
    QAction *settingsAction;
    QAction *quitAction;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;

    QString m_detailedErrorText;
    QIcon m_iconOK;
    QIcon m_iconBad;

    SC_HANDLE m_scManager;
    SC_HANDLE m_mediaServerHandle;
    SC_HANDLE m_appServerHandle;
    
    QAction* m_mediaServerStartAction;
    QAction* m_mediaServerStopAction;
    QAction* m_appServerStartAction;
    QAction* m_appServerStopAction;
    bool m_firstTimeToolTipError;
    QTimer m_findServices;
    QTimer m_updateServiceStatus;

    bool m_needStartMediaServer;
    bool m_needStartAppServer;
    int m_skipTicks;

    typedef QPair<QString, QAction*> NameAndAction;
    typedef QList<NameAndAction> NameActionList;

    int m_prevMediaServerStatus;
    int m_prevAppServerStatus;

    NameActionList m_actionList;
    QTime m_lastMessageTimer;
    QQueue<QString> m_delayedMessages;
    QString m_mediaServerServiceName;
    QString m_appServerServiceName;
    FoundEnterpriseControllersModel* const m_foundEnterpriseControllersModel;
};

#endif
