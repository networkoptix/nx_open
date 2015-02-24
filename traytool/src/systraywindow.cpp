#include <QtCore/QSettings>
#include <QtCore/QThreadPool>
#include <QtCore/QProcess>
#include <QtNetwork/QTcpServer>
#include <QtWidgets/QMenu>
#include <QtWidgets/QApplication>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>

#include "systraywindow.h"

#include <shlobj.h>
#include "version.h"

#pragma comment(lib, "Shell32.lib") /* For IsUserAnAdmin. */
#pragma comment(lib, "AdvApi32.lib") /* For ControlService and other handle-related functions. */


#define USE_SINGLE_STREAMING_PORT

static const QString MEDIA_SERVER_NAME(QString(lit(QN_ORGANIZATION_NAME)) + lit(" Media Server"));

static const int MESSAGE_DURATION = 3 * 1000;
static const int DEFAUT_PROXY_PORT = 7009;

namespace {
    const int findServicesTimeoutMs = 10000;
    const int updateStatusMs = 500;
}

bool MyIsUserAnAdmin()
{
   bool isAdmin = false;

   DWORD bytesUsed = 0;

   TOKEN_ELEVATION_TYPE tokenElevationType;

   HANDLE m_hToken;
   if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &m_hToken) == FALSE)
   {
       return false;
   }

   if (::GetTokenInformation(m_hToken, TokenElevationType, &tokenElevationType, sizeof(tokenElevationType), &bytesUsed) == FALSE)
   {
       if (m_hToken)
           CloseHandle(m_hToken);
       return IsUserAnAdmin();
   }

   isAdmin = tokenElevationType == TokenElevationTypeFull || tokenElevationType == TokenElevationTypeDefault && IsUserAnAdmin();

   if (m_hToken)
       CloseHandle(m_hToken);

   return isAdmin;
}

QnSystrayWindow::QnSystrayWindow()
    : m_mediaServerSettings(QSettings::SystemScope, qApp->organizationName(), MEDIA_SERVER_NAME)
{
    m_mediaServerHandle = 0;

    m_mediaServerServiceName = QString(lit(QN_CUSTOMIZATION_NAME)) + lit("MediaServer");

    m_mediaServerStartAction = 0;
    m_mediaServerStopAction = 0;
    m_mediaServerWebAction = 0;

    m_scManager = 0;
    m_firstTimeToolTipError = true;
    m_needStartMediaServer = false;
    
    m_prevMediaServerStatus = -1;
    m_lastMessageTimer.restart();

    createActions();
    createTrayIcon();

    findServiceInfo();

    setWindowTitle(lit(QN_APPLICATION_DISPLAY_NAME));

    QTimer* findServicesTimer = new QTimer(this);
    connect(findServicesTimer, &QTimer::timeout, this, &QnSystrayWindow::findServiceInfo);
    findServicesTimer->start(findServicesTimeoutMs);

    QTimer* updateStatusTimer = new QTimer(this);
    connect(updateStatusTimer, &QTimer::timeout, this, &QnSystrayWindow::updateServiceInfo);
    updateStatusTimer->start(updateStatusMs);
}

void QnSystrayWindow::handleMessage(const QString& message)
{
    if (message == lit("quit")) {
        qApp->quit();
    } else if (message == lit("activate")) {
        return;
    }
}

QnSystrayWindow::~QnSystrayWindow()
{
    if (m_mediaServerHandle)
        CloseServiceHandle(m_mediaServerHandle);
    if (m_scManager)
        CloseServiceHandle(m_scManager);
}

void QnSystrayWindow::executeAction(const QString &actionName)
{
    QAction* action = actionByName(actionName);
    if (action && action->isEnabled())
        action->activate(QAction::Trigger);
}

void QnSystrayWindow::findServiceInfo() {
    if (m_scManager == NULL)
        m_scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    
    if (m_scManager == NULL)
        m_scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (m_scManager == NULL) 
    {
        int error = GetLastError();
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
                errorText =  tr("Specified parameter is invalid.");
                break;
        }
        if (m_firstTimeToolTipError) {
            showMessage(tr("Could not access installed services"), tr("An error has occurred while trying to access installed services:\n %1").arg(errorText), QSystemTrayIcon::Critical);
            m_firstTimeToolTipError = false;
        }
        return;
    }
    
    if (m_mediaServerHandle == 0)
        m_mediaServerHandle = OpenService(m_scManager, (LPCWSTR) m_mediaServerServiceName.data(), SERVICE_ALL_ACCESS);
    if (m_mediaServerHandle == 0)
        m_mediaServerHandle = OpenService(m_scManager, (LPCWSTR) m_mediaServerServiceName.data(), SERVICE_QUERY_STATUS);

    if (!isServerInstalled() && m_firstTimeToolTipError) {
        showMessage(lit(QN_APPLICATION_DISPLAY_NAME), tr("No %1 services installed").arg(lit(QN_ORGANIZATION_NAME)));
        m_firstTimeToolTipError = false;
    }

    updateActions();
}

void QnSystrayWindow::closeEvent(QCloseEvent *event)
{
    if (m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    }
}

void QnSystrayWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
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

void QnSystrayWindow::showMessage(const QString &title, const QString &msg, QSystemTrayIcon::MessageIcon icon)
{
    if (m_lastMessageTimer.elapsed() >= 1000) {
        m_trayIcon->showMessage(title, msg, icon, MESSAGE_DURATION);
        m_lastMessageTimer.restart();
    }
    else {
        m_delayedMessages << DelayedMessage(title, msg, icon);
        QTimer::singleShot(1500, this, SLOT(onDelayedMessage()));
    }
}

void QnSystrayWindow::onDelayedMessage() {
    if (!m_delayedMessages.isEmpty()) {
        DelayedMessage msg = m_delayedMessages.dequeue();
        showMessage(msg.title, msg.msg, msg.icon); 
    }
}

void QnSystrayWindow::updateServiceInfo() {
    if (!isServerInstalled())
        return;

    GetServiceInfoAsyncTask *mediaServerTask = new GetServiceInfoAsyncTask(m_mediaServerHandle);
    connect(mediaServerTask, SIGNAL(finished(quint64)), this, SLOT(mediaServerInfoUpdated(quint64)), Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(mediaServerTask);
}

void QnSystrayWindow::mediaServerInfoUpdated(quint64 status) {
    if (!isServerInstalled())
        return;

    updateServiceInfoInternal(m_mediaServerHandle, status, m_mediaServerStartAction, m_mediaServerStopAction);
    if (status == SERVICE_STOPPED)
    {
        if (m_needStartMediaServer) 
        {
            m_needStartMediaServer = false;
            StartService(m_mediaServerHandle, 0, 0);
        }

        if (m_prevMediaServerStatus >= 0 && m_prevMediaServerStatus != SERVICE_STOPPED && !m_needStartMediaServer) {
            showMessage(lit(QN_APPLICATION_DISPLAY_NAME), tr("Media server has been stopped"));
        }
    }
    else if (status == SERVICE_RUNNING)
    {
        if (m_prevMediaServerStatus >= 0 && m_prevMediaServerStatus != SERVICE_RUNNING) {
            showMessage(lit(QN_APPLICATION_DISPLAY_NAME), tr("Media server has been started"));
        }
    }
    m_prevMediaServerStatus = status;
}

void QnSystrayWindow::updateServiceInfoInternal(SC_HANDLE handle, DWORD status, QAction *startAction, QAction *stopAction)
{
    if (!handle) {
        stopAction->setVisible(false);
        startAction->setVisible(false);
        return;
    }

    QAction *action = NULL;
    QString suffix;

    switch(status) {
    case SERVICE_STOPPED:
        stopAction->setVisible(false);
        startAction->setVisible(true);
        startAction->setEnabled(true);

        action = startAction;
        suffix = tr(" (stopped)");
        break;
    case SERVICE_START_PENDING:
        stopAction->setVisible(false);
        startAction->setVisible(true);
        startAction->setEnabled(false);

        action = startAction;
        suffix = tr(" (starting)");
        break;
    case SERVICE_STOP_PENDING:
        stopAction->setVisible(true);
        stopAction->setEnabled(false);
        startAction->setVisible(false);

        action = stopAction;
        suffix = tr(" (stopping)");
        break;
    case SERVICE_RUNNING:
        stopAction->setVisible(true);
        stopAction->setEnabled(true);
        startAction->setVisible(false);

        action = stopAction;
        suffix = tr(" (started)");
        break;
    case SERVICE_CONTINUE_PENDING:
        stopAction->setVisible(true);
        stopAction->setEnabled(false);
        startAction->setVisible(false);

        action = stopAction;
        suffix = tr(" (resuming)");
        break;
    case SERVICE_PAUSED:
        stopAction->setVisible(false);
        startAction->setVisible(true);
        startAction->setEnabled(true);

        action = startAction;
        suffix = tr(" (paused)");
        break;
    case SERVICE_PAUSE_PENDING:
        stopAction->setVisible(false);
        startAction->setVisible(true);
        startAction->setEnabled(false);

        action = startAction;
        suffix = tr(" (pausing)");
        break;
    default:
        break;
    }

    if(action) {
        const char *originalTitlePropertyName = "qn_orignalTitle";
        QString title = action->property(originalTitlePropertyName).toString();
        if(title.isEmpty()) {
            title = action->text();
            action->setProperty(originalTitlePropertyName, title);
        }

        action->setText(title + suffix);
    }
}

void QnSystrayWindow::at_mediaServerStartAction() {
    if (!isServerInstalled())
        return;

    StartService(m_mediaServerHandle, 0, 0);
    updateServiceInfo();
}

void QnSystrayWindow::at_mediaServerStopAction() {
    if (!isServerInstalled())
        return;

    if (QMessageBox::question(0, tr(""), tr("Media server is going to be stopped. Are you sure?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        StopServiceAsyncTask *stopTask = new StopServiceAsyncTask(m_mediaServerHandle);
        connect(stopTask, SIGNAL(finished()), this, SLOT(updateServiceInfo()), Qt::QueuedConnection);
        QThreadPool::globalInstance()->start(stopTask);
        updateServiceInfoInternal(m_mediaServerHandle, SERVICE_STOP_PENDING, m_mediaServerStartAction, m_mediaServerStopAction);
    }

}

void QnSystrayWindow::at_mediaServerWebAction() {
    if (!isServerInstalled())
        return;

    QString serverAdminUrl = QString(lit("http://localhost:%1/static/index.html")).arg(m_mediaServerSettings.value(lit("port")).toInt());
    QDesktopServices::openUrl(QUrl(serverAdminUrl));
}

QnElevationChecker::QnElevationChecker(QObject* parent, QString actionName, QObject* target, const char* slot): 
    QObject(parent),
    m_actionName(actionName),
    m_target(target),
    m_slot(slot),
    m_rightWarnShowed(false)
{
    connect(this, SIGNAL(elevationCheckPassed()), target, slot);
}


void QnElevationChecker::triggered()
{
    if (MyIsUserAnAdmin())
    {
        emit elevationCheckPassed();
    }
    else if (qApp->arguments().size() > 1 && !m_rightWarnShowed)
    {
        // already elevated, but not admin. Prevent recursion calls here
        QnSystrayWindow *systray = static_cast<QnSystrayWindow*> (parent());
        // TODO: #TR uac must be enabled? why? maybe disabled?
        systray->showMessage(tr("Insufficient privileges to manage services"), tr("UAC must be enabled to request privileges for non-admin users"), QSystemTrayIcon::Warning);
        m_rightWarnShowed = true;
    }
    else
    {
        wchar_t name[1024];
        wchar_t args[1024];

        int size = qApp->arguments()[0].toWCharArray(name);
        name[size] = 0;

        size = m_actionName.toWCharArray(args);
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

void QnSystrayWindow::connectElevatedAction(QAction* source, const char* signal, QObject* target, const char* slot)
{
    QnElevationChecker* checker = new QnElevationChecker(this, nameByAction(source), target, slot);
    connect(source, signal, checker, SLOT(triggered()));
}

void QnSystrayWindow::createActions() {
    m_quitAction = new QAction(tr("&Quit"), this);
    connect(m_quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    m_showMediaServerLogAction = new QAction(tr("Show &Media Server Log"), this);
    m_showMediaServerLogAction->setObjectName(lit("showMediaServerLog"));
    m_actions.push_back(m_showMediaServerLogAction);
    connectElevatedAction(m_showMediaServerLogAction, SIGNAL(triggered()), this, SLOT(onShowMediaServerLogAction()));

    m_mediaServerStartAction = new QAction(QString(tr("Start Media Server")), this);
    m_mediaServerStartAction->setObjectName(lit("startMediaServer"));
    m_actions.push_back(m_mediaServerStartAction);
    connectElevatedAction(m_mediaServerStartAction, SIGNAL(triggered()), this, SLOT(at_mediaServerStartAction()));
    m_mediaServerStartAction->setVisible(false);

    m_mediaServerStopAction = new QAction(QString(tr("Stop Media Server")), this);
    m_mediaServerStopAction->setObjectName(lit("stopMediaServer"));
    m_actions.push_back(m_mediaServerStopAction);
    connectElevatedAction(m_mediaServerStopAction, SIGNAL(triggered()), this, SLOT(at_mediaServerStopAction()));
    m_mediaServerStopAction->setVisible(false);

    m_mediaServerWebAction = new QAction(QString(tr("Media Server Web Page")), this);
    m_mediaServerWebAction->setObjectName(lit("startMediaServer"));
    m_actions.push_back(m_mediaServerWebAction);
    QObject::connect(m_mediaServerWebAction, SIGNAL(triggered()), this, SLOT(at_mediaServerWebAction()));

    updateActions();
}

void QnSystrayWindow::updateActions() {
    m_showMediaServerLogAction->setEnabled(isServerInstalled());
    m_mediaServerWebAction->setEnabled(isServerInstalled());
}

void QnSystrayWindow::onShowMediaServerLogAction() {
    if (!isServerInstalled())
        return;

    QString logFileName = m_mediaServerSettings.value(lit("logFile")).toString() + lit(".log");
    QProcess::startDetached(lit("notepad ") + logFileName);
};

QAction* QnSystrayWindow::actionByName(const QString &name)
{
    foreach(QAction *action, m_actions)
       if (action->objectName() == name)
           return action;
    return NULL;
}

QString QnSystrayWindow::nameByAction(QAction* action)
{
    return action->objectName();
}

void QnSystrayWindow::createTrayIcon() {
    QMenu* trayIconMenu = new QMenu(this);

    trayIconMenu->addAction(m_mediaServerWebAction);

    trayIconMenu->addSeparator();
    trayIconMenu->addAction(m_mediaServerStartAction);
    trayIconMenu->addAction(m_mediaServerStopAction);
    trayIconMenu->addAction(m_showMediaServerLogAction);

    trayIconMenu->addSeparator();
    trayIconMenu->addAction(m_quitAction);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(trayIconMenu);    
    m_trayIcon->setIcon(QIcon(lit(":/traytool.png")));

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &QnSystrayWindow::iconActivated);
    m_trayIcon->show();
}

bool QnSystrayWindow::isServerInstalled() const {
    return m_mediaServerHandle != 0;
}

QnSystrayWindow::DelayedMessage::DelayedMessage(const QString &title, const QString &msg, QSystemTrayIcon::MessageIcon icon):
    title(title), msg(msg), icon(icon)
{}
