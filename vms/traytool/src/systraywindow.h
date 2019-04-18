#pragma once

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

namespace Ui { class SettingsDialog; }

class QnElevationChecker: public QObject
{
    Q_OBJECT
public:
    explicit QnElevationChecker(
        const QString& actionId,
        QObject* parent = nullptr);

    static bool isUserAnAdmin();

    void trigger();

signals:
    void elevationCheckPassed();

private:
    const QString m_actionId;
    bool m_errorDisplayedOnce = false;
};

class StopServiceAsyncTask: public QObject, public QRunnable
{
Q_OBJECT
public:
    StopServiceAsyncTask(SC_HANDLE handle): m_handle(handle)
    {
    }

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
    GetServiceInfoAsyncTask(SC_HANDLE handle): m_handle(handle)
    {
    }

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

class QnSystrayWindow: public QDialog
{
Q_OBJECT
    typedef QDialog base_type;

public:
    QnSystrayWindow();

    virtual ~QnSystrayWindow();
    void executeAction(const QString& cmd);

    void showMessage(
        const QString& title,
        const QString& msg,
        QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);

    void handleMessage(const QString& message);
protected:
    virtual void closeEvent(QCloseEvent* event) override;

private slots:
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
    void updateServiceInfoInternal(
        SC_HANDLE handle,
        DWORD status,
        QAction* startAction,
        QAction* stopAction);

    void createActions();
    void updateActions();
    void createTrayIcon();

    bool isServerInstalled() const;
private:
    QSettings m_mediaServerSettings;

    QSystemTrayIcon* m_trayIcon = nullptr;

    SC_HANDLE m_scManager = nullptr;
    SC_HANDLE m_mediaServerHandle = nullptr;

    QAction* m_quitAction = nullptr;
    QAction* m_showMediaServerLogAction = nullptr;
    QAction* m_mediaServerStartAction = nullptr;
    QAction* m_mediaServerStopAction = nullptr;
    QAction* m_mediaServerWebAction = nullptr;

    /** Flag if an error was already displayed for the user. */
    bool m_errorDisplayedOnce = false;
    bool m_needStartMediaServer = false;
    int m_prevMediaServerStatus = -1;

    QTime m_lastMessageTimer;

    struct DelayedMessage
    {
        QString title;
        QString msg;
        QSystemTrayIcon::MessageIcon icon;
        DelayedMessage(
            const QString& title,
            const QString& msg,
            QSystemTrayIcon::MessageIcon icon);
    };

    QQueue<DelayedMessage> m_delayedMessages;
    QString m_mediaServerServiceName;
};
