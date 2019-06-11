#include "systraywindow.h"

#include <chrono>
#include <deque>

#include <QtCore/QSettings>
#include <QtCore/QThreadPool>
#include <QtCore/QProcess>
#include <QtNetwork/QTcpServer>
#include <QtWidgets/QMenu>
#include <QtWidgets/QApplication>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>

#include <network/system_helpers.h>
#include <traytool_app_info.h>

#include <nx/utils/app_info.h>

#include <shlobj.h>

using namespace std::chrono;

namespace {

static constexpr milliseconds kMessageDurationMs = 3s;
static constexpr milliseconds kFindServicesTimeoutMs = 10s;
static constexpr milliseconds kUpdateStatusMs = 500ms;

static const QString kStartMediaServerId = "startMediaServer";
static const QString kStopMediaServerId = "stopMediaServer";

class StopServiceAsyncTask: public RunnableTask
{
public:
    StopServiceAsyncTask(SC_HANDLE handle):
        m_handle(handle)
    {
    }

    void run()
    {
        SERVICE_STATUS serviceStatus;
        ControlService(m_handle, SERVICE_CONTROL_STOP, &serviceStatus);
        emit finished(0);
    }

private:
    SC_HANDLE m_handle;
};

class GetServiceInfoAsyncTask: public RunnableTask
{
public:
    GetServiceInfoAsyncTask(SC_HANDLE handle):
        m_handle(handle)
    {
    }

    void run()
    {
        SERVICE_STATUS serviceStatus;
        if (QueryServiceStatus(m_handle, &serviceStatus))
            emit finished((quint64)serviceStatus.dwCurrentState);
    }

private:
    SC_HANDLE m_handle;
};

struct DelayedMessage
{
    QString title;
    QString msg;
    QSystemTrayIcon::MessageIcon icon;

    DelayedMessage(const QString& title, const QString& msg, QSystemTrayIcon::MessageIcon icon) :
        title(title),
        msg(msg),
        icon(icon)
    {
    }

};

} // namespace

struct QnSystrayWindow::Private
{
    Private():
        mediaServerSettings(
            QSettings::SystemScope,
            nx::utils::AppInfo::organizationName(),
            QnTraytoolAppInfo::mediaServerRegistryKey())
    {
    }

    QSettings mediaServerSettings;

    QSystemTrayIcon* trayIcon = nullptr;

    SC_HANDLE scManager = nullptr;
    SC_HANDLE mediaServerHandle = nullptr;

    QAction* quitAction = nullptr;
    QAction* showMediaServerLogAction = nullptr;
    QAction* mediaServerStartAction = nullptr;
    QAction* mediaServerStopAction = nullptr;
    QAction* mediaServerWebAction = nullptr;

    /** Flag if an error was already displayed for the user. */
    bool errorDisplayedOnce = false;
    bool needStartMediaServer = false;
    int prevMediaServerStatus = -1;

    QTime lastMessageTimer;

    std::deque<DelayedMessage> delayedMessages;
    QString mediaServerServiceName;
};

QnSystrayWindow::QnSystrayWindow():
    d(new Private())
{
    d->mediaServerServiceName = nx::utils::AppInfo::customizationName() + "MediaServer";
    d->lastMessageTimer.restart();

    createActions();
    createTrayIcon();

    findServiceInfo();

    setWindowTitle(QnTraytoolAppInfo::applicationName());

    const auto findServicesTimer = new QTimer(this);
    connect(findServicesTimer, &QTimer::timeout, this, &QnSystrayWindow::findServiceInfo);
    findServicesTimer->start(kFindServicesTimeoutMs.count());

    const auto updateStatusTimer = new QTimer(this);
    connect(updateStatusTimer, &QTimer::timeout, this, &QnSystrayWindow::updateServiceInfo);
    updateStatusTimer->start(kUpdateStatusMs.count());
}

void QnSystrayWindow::handleMessage(const QString& message)
{
    if (message == "quit")
        qApp->quit();
}

QnSystrayWindow::~QnSystrayWindow()
{
    if (d->mediaServerHandle)
        CloseServiceHandle(d->mediaServerHandle);
    if (d->scManager)
        CloseServiceHandle(d->scManager);
}

void QnSystrayWindow::executeAction(const QString& actionName)
{
    if (actionName == kStartMediaServerId)
        at_mediaServerStartAction();
    else if (actionName == kStopMediaServerId)
        at_mediaServerStopAction();
}

void QnSystrayWindow::findServiceInfo()
{
    if (!d->scManager)
        d->scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (!d->scManager)
        d->scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (!d->scManager)
    {
        if (!d->errorDisplayedOnce)
        {
            const int error = GetLastError();
            QString errorText;
            switch (error)
            {
                case ERROR_ACCESS_DENIED:
                    errorText = tr("Access denied.");
                    break;
                case ERROR_DATABASE_DOES_NOT_EXIST:
                    errorText = tr("Specified database does not exist.");
                    break;
                case ERROR_INVALID_PARAMETER:
                    errorText = tr("Specified parameter is invalid.");
                    break;
                default:
                    errorText = tr("Unknown error: %1").arg(error);
                    break;
            }

            showMessage(
                tr("Could not access installed services"),
                tr("An error has occurred while trying to access installed services:")
                    + '\n'
                    + errorText,
                QSystemTrayIcon::Critical);
            d->errorDisplayedOnce = true;
        }

        return;
    }

    if (!d->mediaServerHandle)
    {
        d->mediaServerHandle = OpenService(
            d->scManager,
            (LPCWSTR)d->mediaServerServiceName.data(),
            SERVICE_ALL_ACCESS);
    }

    if (!d->mediaServerHandle)
    {
        d->mediaServerHandle = OpenService(
            d->scManager,
            (LPCWSTR)d->mediaServerServiceName.data(),
            SERVICE_QUERY_STATUS);
    }

    if (!isServerInstalled() && !d->errorDisplayedOnce)
    {
        showMessage(
            QnTraytoolAppInfo::applicationName(),
            tr("No %1 services installed").arg(nx::utils::AppInfo::organizationName()));
        d->errorDisplayedOnce = true;
    }

    updateActions();
}

void QnSystrayWindow::closeEvent(QCloseEvent* event)
{
    if (d->trayIcon->isVisible())
    {
        hide();
        event->ignore();
    }
}

void QnSystrayWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
        case QSystemTrayIcon::DoubleClick:
        case QSystemTrayIcon::Trigger:
            d->trayIcon->contextMenu()->popup(QCursor::pos());
            d->trayIcon->contextMenu()->activateWindow();
            break;
        case QSystemTrayIcon::MiddleClick:
            break;
        default:
            break;
    }
}

void QnSystrayWindow::showMessage(
    const QString& title,
    const QString& msg,
    QSystemTrayIcon::MessageIcon icon)
{
    if (d->lastMessageTimer.elapsed() >= 1000)
    {
        d->trayIcon->showMessage(title, msg, icon, kMessageDurationMs.count());
        d->lastMessageTimer.restart();
    }
    else
    {
        d->delayedMessages.emplace_back(title, msg, icon);
        QTimer::singleShot(1500, this, &QnSystrayWindow::onDelayedMessage);
    }
}

void QnSystrayWindow::onDelayedMessage()
{
    if (!d->delayedMessages.empty())
    {
        DelayedMessage msg = d->delayedMessages.front();
        d->delayedMessages.pop_back();
        showMessage(msg.title, msg.msg, msg.icon);
    }
}

void QnSystrayWindow::updateServiceInfo()
{
    if (!isServerInstalled())
        return;

    const auto mediaServerTask = new GetServiceInfoAsyncTask(d->mediaServerHandle);
    connect(
        mediaServerTask,
        &GetServiceInfoAsyncTask::finished,
        this,
        &QnSystrayWindow::mediaServerInfoUpdated,
        Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(mediaServerTask);
}

void QnSystrayWindow::mediaServerInfoUpdated(quint64 status)
{
    if (!isServerInstalled())
        return;

    updateServiceInfoInternal(status);

    if (status == SERVICE_STOPPED)
    {
        if (d->needStartMediaServer)
        {
            d->needStartMediaServer = false;
            StartService(d->mediaServerHandle, 0, 0);
        }

        if (d->prevMediaServerStatus >= 0 && d->prevMediaServerStatus != SERVICE_STOPPED && !
            d->needStartMediaServer)
        {
            showMessage(QnTraytoolAppInfo::applicationName(), tr("Server has been stopped"));
        }
    }
    else if (status == SERVICE_RUNNING)
    {
        if (d->prevMediaServerStatus >= 0 && d->prevMediaServerStatus != SERVICE_RUNNING)
        {
            showMessage(QnTraytoolAppInfo::applicationName(), tr("Server has been started"));
        }
    }
    d->prevMediaServerStatus = status;
}

void QnSystrayWindow::updateServiceInfoInternal(quint64 status)
{
    if (!d->mediaServerHandle)
    {
        d->mediaServerStopAction->setVisible(false);
        d->mediaServerStartAction->setVisible(false);
        return;
    }

    QAction* action = nullptr;
    QString suffix;

    switch (status)
    {
        case SERVICE_STOPPED:
            d->mediaServerStopAction->setVisible(false);
            d->mediaServerStartAction->setVisible(true);
            d->mediaServerStartAction->setEnabled(true);
            action = d->mediaServerStartAction;
            suffix = tr("stopped");
            break;

        case SERVICE_START_PENDING:
            d->mediaServerStopAction->setVisible(false);
            d->mediaServerStartAction->setVisible(true);
            d->mediaServerStartAction->setEnabled(false);
            action = d->mediaServerStartAction;
            suffix = tr("starting");
            break;

        case SERVICE_STOP_PENDING:
            d->mediaServerStopAction->setVisible(true);
            d->mediaServerStopAction->setEnabled(false);
            d->mediaServerStartAction->setVisible(false);
            action = d->mediaServerStopAction;
            suffix = tr("stopping");
            break;

        case SERVICE_RUNNING:
            d->mediaServerStopAction->setVisible(true);
            d->mediaServerStopAction->setEnabled(true);
            d->mediaServerStartAction->setVisible(false);
            action = d->mediaServerStopAction;
            suffix = tr("started");
            break;

        case SERVICE_CONTINUE_PENDING:
            d->mediaServerStopAction->setVisible(true);
            d->mediaServerStopAction->setEnabled(false);
            d->mediaServerStartAction->setVisible(false);
            action = d->mediaServerStopAction;
            suffix = tr("resuming");
            break;

        case SERVICE_PAUSED:
            d->mediaServerStopAction->setVisible(false);
            d->mediaServerStartAction->setVisible(true);
            d->mediaServerStartAction->setEnabled(true);
            action = d->mediaServerStartAction;
            suffix = tr("paused");
            break;

        case SERVICE_PAUSE_PENDING:
            d->mediaServerStopAction->setVisible(false);
            d->mediaServerStartAction->setVisible(true);
            d->mediaServerStartAction->setEnabled(false);
            action = d->mediaServerStartAction;
            suffix = tr("pausing");
            break;

        default:
            break;
    }

    if (action)
    {
        static const char* kOriginalTitlePropertyName = "qn_orignalTitle";
        QString title = action->property(kOriginalTitlePropertyName).toString();
        if (title.isEmpty())
        {
            title = action->text();
            action->setProperty(kOriginalTitlePropertyName, title);
        }

        if (!suffix.isEmpty())
            title = QString("%1 (%2)").arg(title).arg(suffix);
        action->setText(title);
    }
}

void QnSystrayWindow::at_mediaServerStartAction()
{
    if (!isServerInstalled())
        return;

    StartService(d->mediaServerHandle, 0, 0);
    updateServiceInfo();
}

void QnSystrayWindow::at_mediaServerStopAction()
{
    if (!isServerInstalled())
        return;

    if (QMessageBox::question(
        nullptr,
        QnTraytoolAppInfo::applicationName(),
        tr("Server will be stopped. Continue?"),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        const auto stopTask = new StopServiceAsyncTask(d->mediaServerHandle);
        connect(
            stopTask,
            &StopServiceAsyncTask::finished,
            this,
            &QnSystrayWindow::updateServiceInfo,
            Qt::QueuedConnection);

        QThreadPool::globalInstance()->start(stopTask);
        updateServiceInfoInternal(SERVICE_STOP_PENDING);
    }
}

void QnSystrayWindow::at_mediaServerWebAction()
{
    if (!isServerInstalled())
        return;

    int port = d->mediaServerSettings.value("port").toInt();
    if (port == 0)
        port = helpers::kDefaultConnectionPort; // Should never get there, but better than 0.

    QString serverAdminUrl = QString("http://localhost:%1/static/index.html").arg(port);
    QDesktopServices::openUrl(QUrl(serverAdminUrl));
}

QnElevationChecker::QnElevationChecker(const QString& actionId, QObject* parent):
    QObject(parent),
    m_actionId(actionId)
{
}

bool QnElevationChecker::isUserAnAdmin()
{
    bool isAdmin = false;

    DWORD bytesUsed = 0;

    TOKEN_ELEVATION_TYPE tokenElevationType;

    HANDLE m_hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &m_hToken) == FALSE)
    {
        return false;
    }

    if (::GetTokenInformation(
        m_hToken,
        TokenElevationType,
        &tokenElevationType,
        sizeof(tokenElevationType),
        &bytesUsed) == FALSE)
    {
        if (m_hToken)
            CloseHandle(m_hToken);
        return IsUserAnAdmin();
    }

    isAdmin = tokenElevationType == TokenElevationTypeFull || tokenElevationType ==
        TokenElevationTypeDefault && IsUserAnAdmin();

    if (m_hToken)
        CloseHandle(m_hToken);

    return isAdmin;
}


void QnElevationChecker::trigger()
{
    if (isUserAnAdmin())
    {
        emit elevationCheckPassed();
    }
    else if (qApp->arguments().size() > 1 && !m_errorDisplayedOnce)
    {
        // Already elevated, but not admin. Prevent recursion calls here
        const auto systray = static_cast<QnSystrayWindow*>(parent());
        systray->showMessage(
            tr("Insufficient rights to manage services."),
            tr("UAC must be enabled to request privileges for non-admin users."),
            QSystemTrayIcon::Warning);
        m_errorDisplayedOnce = true;
    }
    else
    {
        wchar_t name[1024];
        wchar_t args[1024];

        int size = qApp->arguments()[0].toWCharArray(name);
        name[size] = 0;

        size = m_actionId.toWCharArray(args);
        args[size] = 0;

        SHELLEXECUTEINFO shExecInfo;

        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);

        shExecInfo.fMask = 0;
        shExecInfo.hwnd = 0;
        shExecInfo.lpVerb = L"runas";
        shExecInfo.lpFile = name;
        shExecInfo.lpParameters = args;
        shExecInfo.lpDirectory = 0;
        shExecInfo.nShow = SW_NORMAL;
        shExecInfo.hInstApp = 0;

        // In case of success new process will send us "quit" signal
        ShellExecuteEx(&shExecInfo);
    }
}

void QnSystrayWindow::createActions()
{
    d->quitAction = new QAction(tr("&Quit"), this);
    connect(d->quitAction, &QAction::triggered, qApp, &QApplication::quit);

    d->showMediaServerLogAction = new QAction(tr("Show Server Log"), this);

    connect(
        d->showMediaServerLogAction,
        &QAction::triggered,
        this,
        &QnSystrayWindow::onShowMediaServerLogAction);

    {
        d->mediaServerStartAction = new QAction(QString(tr("Start Server")), this);
        const auto checker = new QnElevationChecker(kStartMediaServerId, this);
        connect(d->mediaServerStartAction, &QAction::triggered, checker,
            &QnElevationChecker::trigger);
        connect(checker, &QnElevationChecker::elevationCheckPassed, this,
            &QnSystrayWindow::at_mediaServerStartAction);
    }

    {
        d->mediaServerStopAction = new QAction(QString(tr("Stop Server")), this);
        const auto checker = new QnElevationChecker(kStopMediaServerId, this);
        connect(d->mediaServerStopAction, &QAction::triggered, checker,
            &QnElevationChecker::trigger);
        connect(checker, &QnElevationChecker::elevationCheckPassed, this,
            &QnSystrayWindow::at_mediaServerStopAction);
        d->mediaServerStopAction->setVisible(false);
    }

    d->mediaServerWebAction = new QAction(QString(tr("Server Web Page")), this);
    connect(
        d->mediaServerWebAction,
        &QAction::triggered,
        this,
        &QnSystrayWindow::at_mediaServerWebAction);

    updateActions();
}

void QnSystrayWindow::updateActions()
{
    d->showMediaServerLogAction->setEnabled(isServerInstalled());
    d->mediaServerWebAction->setEnabled(isServerInstalled());
}

void QnSystrayWindow::onShowMediaServerLogAction()
{
    if (!isServerInstalled())
        return;

    QString logFileName = d->mediaServerSettings.value("logFile").toString() + ".log";
    QProcess::startDetached("notepad " + logFileName);
};

void QnSystrayWindow::createTrayIcon()
{
    auto trayIconMenu = new QMenu(this);

    trayIconMenu->addAction(d->mediaServerWebAction);

    trayIconMenu->addSeparator();
    trayIconMenu->addAction(d->mediaServerStartAction);
    trayIconMenu->addAction(d->mediaServerStopAction);
    trayIconMenu->addAction(d->showMediaServerLogAction);

    trayIconMenu->addSeparator();
    trayIconMenu->addAction(d->quitAction);

    d->trayIcon = new QSystemTrayIcon(this);
    d->trayIcon->setContextMenu(trayIconMenu);
    d->trayIcon->setIcon(QIcon(":/logo.png"));

    connect(d->trayIcon, &QSystemTrayIcon::activated, this, &QnSystrayWindow::iconActivated);
    d->trayIcon->show();
}

bool QnSystrayWindow::isServerInstalled() const
{
    return d->mediaServerHandle != 0;
}
