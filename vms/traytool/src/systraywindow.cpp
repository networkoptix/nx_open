#include "systraywindow.h"

#include <chrono>

#include <QtCore/QSettings>
#include <QtCore/QThreadPool>
#include <QtCore/QProcess>
#include <QtNetwork/QTcpServer>
#include <QtWidgets/QMenu>
#include <QtWidgets/QApplication>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>

#include <shlobj.h>

#include <network/system_helpers.h>
#include <traytool_app_info.h>

#include <nx/utils/app_info.h>

using namespace std::chrono;

namespace {

static constexpr milliseconds kMessageDurationMs = 3s;
static constexpr milliseconds kFindServicesTimeoutMs = 10s;
static constexpr milliseconds kUpdateStatusMs = 500ms;

static const QString kStartMediaServerId = "startMediaServer";
static const QString kStopMediaServerId = "stopMediaServer";

} // namespace

QnSystrayWindow::QnSystrayWindow():
    m_mediaServerSettings(
        QSettings::SystemScope,
        nx::utils::AppInfo::organizationName(),
        QnTraytoolAppInfo::mediaServerRegistryKey())
{
    m_mediaServerServiceName = nx::utils::AppInfo::customizationName() + "MediaServer";
    m_lastMessageTimer.restart();

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
    if (m_mediaServerHandle)
        CloseServiceHandle(m_mediaServerHandle);
    if (m_scManager)
        CloseServiceHandle(m_scManager);
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
    if (!m_scManager)
        m_scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (!m_scManager)
        m_scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (!m_scManager)
    {
        if (!m_errorDisplayedOnce)
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
            m_errorDisplayedOnce = true;
        }

        return;
    }

    if (!m_mediaServerHandle)
    {
        m_mediaServerHandle = OpenService(
            m_scManager,
            (LPCWSTR)m_mediaServerServiceName.data(),
            SERVICE_ALL_ACCESS);
    }

    if (!m_mediaServerHandle)
    {
        m_mediaServerHandle = OpenService(
            m_scManager,
            (LPCWSTR)m_mediaServerServiceName.data(),
            SERVICE_QUERY_STATUS);
    }

    if (!isServerInstalled() && !m_errorDisplayedOnce)
    {
        showMessage(
            QnTraytoolAppInfo::applicationName(),
            tr("No %1 services installed").arg(nx::utils::AppInfo::organizationName()));
        m_errorDisplayedOnce = true;
    }

    updateActions();
}

void QnSystrayWindow::closeEvent(QCloseEvent* event)
{
    if (m_trayIcon->isVisible())
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
            m_trayIcon->contextMenu()->popup(QCursor::pos());
            m_trayIcon->contextMenu()->activateWindow();
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
    if (m_lastMessageTimer.elapsed() >= 1000)
    {
        m_trayIcon->showMessage(title, msg, icon, kMessageDurationMs.count());
        m_lastMessageTimer.restart();
    }
    else
    {
        m_delayedMessages << DelayedMessage(title, msg, icon);
        QTimer::singleShot(1500, this, &QnSystrayWindow::onDelayedMessage);
    }
}

void QnSystrayWindow::onDelayedMessage()
{
    if (!m_delayedMessages.isEmpty())
    {
        DelayedMessage msg = m_delayedMessages.dequeue();
        showMessage(msg.title, msg.msg, msg.icon);
    }
}

void QnSystrayWindow::updateServiceInfo()
{
    if (!isServerInstalled())
        return;

    const auto mediaServerTask = new GetServiceInfoAsyncTask(m_mediaServerHandle);
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

    updateServiceInfoInternal(
        m_mediaServerHandle,
        status,
        m_mediaServerStartAction,
        m_mediaServerStopAction);

    if (status == SERVICE_STOPPED)
    {
        if (m_needStartMediaServer)
        {
            m_needStartMediaServer = false;
            StartService(m_mediaServerHandle, 0, 0);
        }

        if (m_prevMediaServerStatus >= 0 && m_prevMediaServerStatus != SERVICE_STOPPED && !
            m_needStartMediaServer)
        {
            showMessage(QnTraytoolAppInfo::applicationName(), tr("Server has been stopped"));
        }
    }
    else if (status == SERVICE_RUNNING)
    {
        if (m_prevMediaServerStatus >= 0 && m_prevMediaServerStatus != SERVICE_RUNNING)
        {
            showMessage(QnTraytoolAppInfo::applicationName(), tr("Server has been started"));
        }
    }
    m_prevMediaServerStatus = status;
}

void QnSystrayWindow::updateServiceInfoInternal(
    SC_HANDLE handle,
    DWORD status,
    QAction* startAction,
    QAction* stopAction)
{
    if (!handle)
    {
        stopAction->setVisible(false);
        startAction->setVisible(false);
        return;
    }

    QAction* action = nullptr;
    QString suffix;

    switch (status)
    {
        case SERVICE_STOPPED:
            stopAction->setVisible(false);
            startAction->setVisible(true);
            startAction->setEnabled(true);
            action = startAction;
            suffix = tr("stopped");
            break;

        case SERVICE_START_PENDING:
            stopAction->setVisible(false);
            startAction->setVisible(true);
            startAction->setEnabled(false);
            action = startAction;
            suffix = tr("starting");
            break;

        case SERVICE_STOP_PENDING:
            stopAction->setVisible(true);
            stopAction->setEnabled(false);
            startAction->setVisible(false);
            action = stopAction;
            suffix = tr("stopping");
            break;

        case SERVICE_RUNNING:
            stopAction->setVisible(true);
            stopAction->setEnabled(true);
            startAction->setVisible(false);
            action = stopAction;
            suffix = tr("started");
            break;

        case SERVICE_CONTINUE_PENDING:
            stopAction->setVisible(true);
            stopAction->setEnabled(false);
            startAction->setVisible(false);
            action = stopAction;
            suffix = tr("resuming");
            break;

        case SERVICE_PAUSED:
            stopAction->setVisible(false);
            startAction->setVisible(true);
            startAction->setEnabled(true);
            action = startAction;
            suffix = tr("paused");
            break;

        case SERVICE_PAUSE_PENDING:
            stopAction->setVisible(false);
            startAction->setVisible(true);
            startAction->setEnabled(false);
            action = startAction;
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

    StartService(m_mediaServerHandle, 0, 0);
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
        const auto stopTask = new StopServiceAsyncTask(m_mediaServerHandle);
        connect(
            stopTask,
            &StopServiceAsyncTask::finished,
            this,
            &QnSystrayWindow::updateServiceInfo,
            Qt::QueuedConnection);

        QThreadPool::globalInstance()->start(stopTask);
        updateServiceInfoInternal(
            m_mediaServerHandle,
            SERVICE_STOP_PENDING,
            m_mediaServerStartAction,
            m_mediaServerStopAction);
    }
}

void QnSystrayWindow::at_mediaServerWebAction()
{
    if (!isServerInstalled())
        return;

    int port = m_mediaServerSettings.value("port").toInt();
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
    m_quitAction = new QAction(tr("&Quit"), this);
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_showMediaServerLogAction = new QAction(tr("Show Server Log"), this);

    connect(
        m_showMediaServerLogAction,
        &QAction::triggered,
        this,
        &QnSystrayWindow::onShowMediaServerLogAction);

    {
        m_mediaServerStartAction = new QAction(QString(tr("Start Server")), this);
        const auto checker = new QnElevationChecker(kStartMediaServerId, this);
        connect(m_mediaServerStartAction, &QAction::triggered, checker,
            &QnElevationChecker::trigger);
        connect(checker, &QnElevationChecker::elevationCheckPassed, this,
            &QnSystrayWindow::at_mediaServerStartAction);
    }

    {
        m_mediaServerStopAction = new QAction(QString(tr("Stop Server")), this);
        const auto checker = new QnElevationChecker(kStopMediaServerId, this);
        connect(m_mediaServerStopAction, &QAction::triggered, checker,
            &QnElevationChecker::trigger);
        connect(checker, &QnElevationChecker::elevationCheckPassed, this,
            &QnSystrayWindow::at_mediaServerStopAction);
        m_mediaServerStopAction->setVisible(false);
    }

    m_mediaServerWebAction = new QAction(QString(tr("Server Web Page")), this);
    connect(
        m_mediaServerWebAction,
        &QAction::triggered,
        this,
        &QnSystrayWindow::at_mediaServerWebAction);

    updateActions();
}

void QnSystrayWindow::updateActions()
{
    m_showMediaServerLogAction->setEnabled(isServerInstalled());
    m_mediaServerWebAction->setEnabled(isServerInstalled());
}

void QnSystrayWindow::onShowMediaServerLogAction()
{
    if (!isServerInstalled())
        return;

    QString logFileName = m_mediaServerSettings.value(lit("logFile")).toString() + lit(".log");
    QProcess::startDetached(lit("notepad ") + logFileName);
};

void QnSystrayWindow::createTrayIcon()
{
    auto trayIconMenu = new QMenu(this);

    trayIconMenu->addAction(m_mediaServerWebAction);

    trayIconMenu->addSeparator();
    trayIconMenu->addAction(m_mediaServerStartAction);
    trayIconMenu->addAction(m_mediaServerStopAction);
    trayIconMenu->addAction(m_showMediaServerLogAction);

    trayIconMenu->addSeparator();
    trayIconMenu->addAction(m_quitAction);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(trayIconMenu);
    m_trayIcon->setIcon(QIcon(lit(":/logo.png")));

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &QnSystrayWindow::iconActivated);
    m_trayIcon->show();
}

bool QnSystrayWindow::isServerInstalled() const
{
    return m_mediaServerHandle != 0;
}

QnSystrayWindow::DelayedMessage::DelayedMessage(
    const QString& title,
    const QString& msg,
    QSystemTrayIcon::MessageIcon icon):
    title(title),
    msg(msg),
    icon(icon)
{
}
