#include <QtGui>

#include "systraywindow.h"
#include "ui_settings.h"
#include "connectiontestingdialog.h"

#include <shlobj.h>


static const QString MEDIA_SERVER_NAME ("VMS Media Server");
static const QString APP_SERVER_NAME("VMS Application Server");
static const int DEFAULT_APP_SERVER_PORT = 8000;
static const int MESSAGE_DURATION = 3 * 1000;

bool MyIsUserAnAdmin()
{
   bool isAdmin = false;

   DWORD bytesUsed = 0;

   TOKEN_ELEVATION_TYPE tokenElevationType;

   HANDLE m_hToken;
   OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &m_hToken);

   ::GetTokenInformation(m_hToken, TokenElevationType, &tokenElevationType, sizeof(tokenElevationType), &bytesUsed);

   isAdmin = tokenElevationType == TokenElevationTypeFull || tokenElevationType == TokenElevationTypeDefault && IsUserAnAdmin();

   if (m_hToken)
       CloseHandle(m_hToken);

   return isAdmin;
}

QnSystrayWindow::QnSystrayWindow():
    ui(new Ui::SettingsDialog),
    m_mServerSettings(QSettings::SystemScope, qApp->organizationName(), MEDIA_SERVER_NAME),
    m_appServerSettings(QSettings::SystemScope, qApp->organizationName(), APP_SERVER_NAME)
{
    ui->setupUi(this);

    m_iconOK = QIcon(":/images/traytool.png");
    m_iconBad = QIcon(":/images/traytool.png");

    m_mediaServerHandle = 0;
    m_appServerHandle = 0;
    m_skipTicks = 0;

    m_mediaServerStartAction = 0;
    m_mediaServerStopAction = 0;
    m_appServerStartAction = 0;
    m_appServerStopAction = 0;
    m_scManager = 0;
    m_firstTimeToolTipError = true;
    m_needStartMediaServer = false;
    m_needStartAppServer = false;
    m_waitingMediaServerStopping = false;
    m_waitingAppServerStopping = false;
    m_waitingAppServerStopping = false;
    m_waitingMediaServerStarted = false;
    m_waitingAppServerStarted = false;

    findServiceInfo();

    createActions();
    createTrayIcon();


    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton * )), this, SLOT(buttonClicked(QAbstractButton * )));

    connect(trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    connect(ui->testButton, SIGNAL(clicked()), this, SLOT(onTestButtonClicked()));

    trayIcon->show();

    setWindowTitle(tr("VMS settings"));
    resize(400, 300);

    connect (&m_findServices, SIGNAL(timeout()), this, SLOT(findServiceInfo()));
    connect (&m_updateServiceStatus, SIGNAL(timeout()), this, SLOT(updateServiceInfo()));
    m_findServices.start(10000);
    m_updateServiceStatus.start(500);
}

void QnSystrayWindow::handleMessage(const QString& message)
{
    if (message == "quit")
        qApp->quit();
    else if (message == "activate")
    {
        trayIcon->showMessage("Network Optix", "I'm here", QSystemTrayIcon::Information, MESSAGE_DURATION);
    }
}

QnSystrayWindow::~QnSystrayWindow()
{
    if (m_mediaServerHandle)
        CloseServiceHandle(m_mediaServerHandle);
    if (m_appServerHandle)
        CloseServiceHandle(m_appServerHandle);
    if (m_scManager)
        CloseServiceHandle(m_scManager);
}

void QnSystrayWindow::executeAction(QString actionName)
{
    QAction* action = actionByName(actionName);
    if (action)
        action->activate(QAction::Trigger);
}

void QnSystrayWindow::findServiceInfo()
{
    if (m_scManager == NULL)
        m_scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    
    if (m_scManager == NULL)
        m_scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (m_scManager == NULL) 
    {
        int error = GetLastError();
        switch (error)
        {
            case ERROR_ACCESS_DENIED:
                m_detailedErrorText = "The requested access was denied";
                break;
            case ERROR_DATABASE_DOES_NOT_EXIST:
                m_detailedErrorText = "The specified database does not exist.";
                break;
            case ERROR_INVALID_PARAMETER:
                m_detailedErrorText =  "A specified parameter is invalid.";
                break;
        }
        if (m_firstTimeToolTipError) {
            trayIcon->showMessage("Insufficient permissions to start/stop services", m_detailedErrorText, QSystemTrayIcon::Critical, MESSAGE_DURATION);
            trayIcon->setIcon(m_iconBad);
            m_firstTimeToolTipError = false;
        }
        return;
    }
    
    if (m_mediaServerHandle == 0)
        m_mediaServerHandle = OpenService(m_scManager, L"VmsMediaServer", SERVICE_ALL_ACCESS);
    if (m_mediaServerHandle == 0)
        m_mediaServerHandle = OpenService(m_scManager, L"VmsMediaServer", SERVICE_QUERY_STATUS);

    if (m_appServerHandle == 0)
        m_appServerHandle  = OpenService(m_scManager, L"VmsAppServer",   SERVICE_ALL_ACCESS);
    if (m_appServerHandle == 0)
        m_appServerHandle  = OpenService(m_scManager, L"VmsAppServer",   SERVICE_QUERY_STATUS);
}

void QnSystrayWindow::setVisible(bool visible)
{
    //minimizeAction->setEnabled(visible);
    //maximizeAction->setEnabled(!isMaximized());
    settingsAction->setEnabled(isMaximized() || !visible);
    QDialog::setVisible(visible);
}

void QnSystrayWindow::closeEvent(QCloseEvent *event)
{
    if (trayIcon->isVisible()) 
    {
        /*
        QMessageBox::information(this, tr("Systray"),
                                 tr("The program will keep running in the "
                                    "system tray. To terminate the program, "
                                    "choose <b>Quit</b> in the context menu "
                                    "of the system tray entry."));
       */
        hide();
        event->ignore();
    }
}

void QnSystrayWindow::setIcon(int index)
{
    //QIcon icon = iconComboBox->itemIcon(index);
    //trayIcon->setIcon(icon);
    //setWindowIcon(icon);

    //trayIcon->setToolTip(iconComboBox->itemText(index));
}


void QnSystrayWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    CURSORINFO  pci;
    pci.cbSize = sizeof(CURSORINFO);

    switch (reason) 
    {
       case QSystemTrayIcon::DoubleClick:
       case QSystemTrayIcon::Trigger:
           if (GetCursorInfo(&pci))
               trayIcon->contextMenu()->popup(QPoint(pci.ptScreenPos.x, pci.ptScreenPos.y));
           break;
       case QSystemTrayIcon::MiddleClick:
           break;
       default:
           ;
    }
}

void QnSystrayWindow::showMessage()
{
    /*
    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::MessageIcon(
            typeComboBox->itemData(typeComboBox->currentIndex()).toInt());
    trayIcon->showMessage(titleEdit->text(), bodyEdit->toPlainText(), icon,
                          durationSpinBox->value() * 1000);
       */
}

void QnSystrayWindow::messageClicked()
{
    //QMessageBox::information(0, qApp->applicationName(), m_detailedErrorText);
      
}

void QnSystrayWindow::updateServiceInfo()
{
    if (m_skipTicks > 0) {
        m_skipTicks--;
        return;
    }

    int mediaServerStatus = updateServiceInfoInternal(m_mediaServerHandle, MEDIA_SERVER_NAME,       m_mediaServerStartAction, m_mediaServerStopAction, m_showMediaServerLogAction);
    int appServerStatus = updateServiceInfoInternal(m_appServerHandle,   APP_SERVER_NAME, m_appServerStartAction,   m_appServerStopAction, m_showAppLogAction);

    if (appServerStatus == SERVICE_STOPPED) 
    {
        if (m_needStartAppServer)
        {
            m_needStartAppServer = false;
            StartService(m_appServerHandle, NULL, 0);
        }
        if (m_waitingAppServerStopping)
        {
            m_waitingAppServerStopping = false;
            QString message = APP_SERVER_NAME + QString(" has been stopped");
            trayIcon->showMessage(message, message, QSystemTrayIcon::Information, MESSAGE_DURATION);
        }
    }
    else if (appServerStatus == SERVICE_RUNNING)
    {
        if (m_waitingAppServerStarted)
        {
            m_waitingAppServerStarted = false;
            QString message = APP_SERVER_NAME + QString(" has been started");
            trayIcon->showMessage(message, message, QSystemTrayIcon::Information, MESSAGE_DURATION);
            if (m_waitingMediaServerStarted) {
                m_skipTicks = 2;
                return;
            }
        }
    }

    if (mediaServerStatus == SERVICE_STOPPED)
    {
        if (m_needStartMediaServer) 
        {
            m_needStartMediaServer = false;
            StartService(m_mediaServerHandle, NULL, 0);
        }
        if (m_waitingMediaServerStopping)
        {
            m_waitingMediaServerStopping = false;
            QString message = MEDIA_SERVER_NAME + QString(" has been stopped");
            trayIcon->showMessage(message, message, QSystemTrayIcon::Information, MESSAGE_DURATION);
        }
    }
    else if (mediaServerStatus == SERVICE_RUNNING)
    {
        if (m_waitingMediaServerStarted)
        {
            m_waitingMediaServerStarted = false;
            QString message = MEDIA_SERVER_NAME + QString(" has been started");
            trayIcon->showMessage(message, message, QSystemTrayIcon::Information, MESSAGE_DURATION);
        }
    }
}

int QnSystrayWindow::updateServiceInfoInternal(SC_HANDLE service, const QString& serviceName, QAction* startAction, QAction* stopAction, QAction* logAction)
{
    SERVICE_STATUS serviceStatus;

    if (!service)
    {
        stopAction->setVisible(false);
        startAction->setVisible(false);
        logAction->setVisible(false);
        return -1;
    }
    logAction->setVisible(true);

    QString str;

    if (QueryServiceStatus(service, &serviceStatus))
    {
        switch(serviceStatus.dwCurrentState)
        {
        case SERVICE_STOPPED:
            stopAction->setVisible(false);
            startAction->setVisible(true);
            startAction->setEnabled(true);
            startAction->setText(tr("Start ") + serviceName + tr(" (stopped)"));
            break;
        case SERVICE_START_PENDING:
            stopAction->setVisible(false);
            startAction->setVisible(true);
            startAction->setEnabled(false);
            startAction->setText(tr("Start ") + serviceName + tr(" (starting)"));
            break;
        case SERVICE_STOP_PENDING:
            stopAction->setVisible(true);
            stopAction->setEnabled(false);
            startAction->setVisible(false);
            stopAction->setText(tr("Stop ") + serviceName + tr(" (stopping)"));
            break;
        case SERVICE_RUNNING:
            stopAction->setVisible(true);
            stopAction->setEnabled(true);
            startAction->setVisible(false);
            stopAction->setText(tr("Stop ") + serviceName + tr(" (started)"));
            break;

        case SERVICE_CONTINUE_PENDING:
            stopAction->setVisible(true);
            stopAction->setEnabled(false);
            startAction->setVisible(false);
            stopAction->setText(tr("Stop ") + serviceName + tr(" (resuming)"));
            break;

        case SERVICE_PAUSED:
            stopAction->setVisible(false);
            startAction->setVisible(true);
            startAction->setEnabled(true);
            startAction->setText(tr("Start ") + serviceName + tr(" (paused)"));
            break;
        case SERVICE_PAUSE_PENDING:
            stopAction->setVisible(false);
            startAction->setVisible(true);
            startAction->setEnabled(false);
            startAction->setText(tr("Start ") + serviceName + tr(" (pausing)"));
            break;
        }
    }
    return serviceStatus.dwCurrentState;
}

void QnSystrayWindow::at_mediaServerStartAction()
{
    if (m_mediaServerHandle) {
        StartService(m_mediaServerHandle, NULL, 0);
        m_waitingMediaServerStarted = true;
        updateServiceInfo();
    }
}

void QnSystrayWindow::at_mediaServerStopAction()
{
    SERVICE_STATUS serviceStatus;
    if (m_mediaServerHandle) 
    {
        if (QMessageBox::question(0, tr("Systray"), "Media server is going to be stopped. Are you sure?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            ControlService(m_mediaServerHandle, SERVICE_CONTROL_STOP, &serviceStatus);
            m_waitingMediaServerStopping = true;
            updateServiceInfo();
        }
    }
}

void QnSystrayWindow::at_appServerStartAction()
{
    if (m_appServerHandle) {
        StartService(m_appServerHandle, NULL, 0);
        m_waitingAppServerStarted = true;
    }
}

void QnSystrayWindow::at_appServerStopAction()
{
    SERVICE_STATUS serviceStatus;
    if (m_appServerHandle) 
    {
        if (QMessageBox::question(0, tr("Systray"), "Application server is going to be stopped. Are you sure?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            ControlService(m_appServerHandle, SERVICE_CONTROL_STOP, &serviceStatus);
            m_waitingAppServerStopping = true;
            updateServiceInfo();
        }
    }
}

QnElevationChecker::QnElevationChecker(QObject* parent, QString actionName, QObject* target, const char* slot)
    : QObject(parent),
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
    else if (QApplication::argc() > 1 && !m_rightWarnShowed)
    {
        // already elevated, but not admin. Prevent recursion calls here
        QnSystrayWindow* systray = static_cast<QnSystrayWindow*> (parent());
        systray->getTrayIcon()->showMessage(tr("Insufficient privileges to manage services"), tr("UAC must be enabled to request privileges for non-admin users"), QSystemTrayIcon::Warning, MESSAGE_DURATION);
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

        shExecInfo.fMask = NULL;
        shExecInfo.hwnd = NULL;
        shExecInfo.lpVerb = L"runas";
        shExecInfo.lpFile = name;
        shExecInfo.lpParameters = args;
        shExecInfo.lpDirectory = NULL;
        shExecInfo.nShow = SW_NORMAL;
        shExecInfo.hInstApp = NULL;

        // In case of success new process will send us "quit" signal
        ShellExecuteEx(&shExecInfo);
    }
}

void QnSystrayWindow::connectElevatedAction(QAction* source, const char* signal, QObject* target, const char* slot)
{
    QnElevationChecker* checker = new QnElevationChecker(this, nameByAction(source), target, slot);
    connect(source, signal, checker, SLOT(triggered()));
}

void QnSystrayWindow::createActions()
{
    settingsAction = new QAction(tr("&Settings"), this);
    m_actionList.append(NameAndAction("showSettings", settingsAction));
    connectElevatedAction(settingsAction, SIGNAL(triggered()), this, SLOT(onSettingsAction()));

    m_showMediaServerLogAction = new QAction(tr("&Show Media server log"), this);
    m_actionList.append(NameAndAction("showMediaServerLog", m_showMediaServerLogAction));
    connectElevatedAction(m_showMediaServerLogAction, SIGNAL(triggered()), this, SLOT(onShowMediaServerLogAction()));

    m_showAppLogAction = new QAction(tr("&Show Application server log"), this);
    m_actionList.append(NameAndAction("showAppServerLog", m_showAppLogAction));
    connectElevatedAction(m_showAppLogAction, SIGNAL(triggered()), this, SLOT(onShowAppServerLogAction()));

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    m_mediaServerStartAction = new QAction(QString(tr("Start ") + MEDIA_SERVER_NAME), this);
    m_actionList.append(NameAndAction("startMediaServer",m_mediaServerStartAction));
    connectElevatedAction(m_mediaServerStartAction, SIGNAL(triggered()), this, SLOT(at_mediaServerStartAction()));

    m_mediaServerStopAction = new QAction(QString(tr("Stop ") + MEDIA_SERVER_NAME), this);
    m_actionList.append(NameAndAction("stopMediaServer", m_mediaServerStopAction));
    connectElevatedAction(m_mediaServerStopAction, SIGNAL(triggered()), this, SLOT(at_mediaServerStopAction()));

    m_appServerStartAction = new QAction(QString(tr("Start ") + APP_SERVER_NAME), this);
    m_actionList.append(NameAndAction("startAppServer", m_appServerStartAction));
    connectElevatedAction(m_appServerStartAction, SIGNAL(triggered()), this, SLOT(at_appServerStartAction()));

    m_appServerStopAction = new QAction(QString(tr("Stop ") + APP_SERVER_NAME), this);
    m_actionList.append(NameAndAction("stopAppServer", m_appServerStopAction));
    connectElevatedAction(m_appServerStopAction, SIGNAL(triggered()), this, SLOT(at_appServerStopAction()));

    updateServiceInfo();
}

 QAction* QnSystrayWindow::actionByName(const QString& name)
 {
     foreach(NameAndAction nameAction, m_actionList)
     {
        if (nameAction.first == name)
            return nameAction.second;
     }

     return 0;
 }

 QString QnSystrayWindow::nameByAction(QAction* action)
 {
     foreach(NameAndAction nameAction, m_actionList)
     {
        if (nameAction.second == action)
            return nameAction.first;
     }

     return "";
 }

void QnSystrayWindow::createTrayIcon()
{
    trayIconMenu = new QMenu();

    trayIconMenu->addAction(m_mediaServerStartAction);
    trayIconMenu->addAction(m_mediaServerStopAction);
    trayIconMenu->addAction(m_appServerStartAction);
    trayIconMenu->addAction(m_appServerStopAction);
    trayIconMenu->addSeparator();

    trayIconMenu->addAction(settingsAction);
    trayIconMenu->addSeparator();

    trayIconMenu->addAction(m_showMediaServerLogAction);
    trayIconMenu->addAction(m_showAppLogAction);
    trayIconMenu->addSeparator();

    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);    
    trayIcon->setIcon(m_iconOK);
}

QUrl QnSystrayWindow::getAppServerURL() const
{
    QUrl appServerUrl;
    appServerUrl.setScheme("http");
    appServerUrl.setHost(m_mServerSettings.value("appserverHost").toString());
    int appServerPort = m_mServerSettings.value("appserverPort").toInt();
    appServerUrl.setPort(appServerPort != 0 ? appServerPort : DEFAULT_APP_SERVER_PORT);
    return appServerUrl;
}

void QnSystrayWindow::setAppServerURL(const QUrl& url)
{
    m_mServerSettings.setValue("appserverHost", url.host());
    m_mServerSettings.setValue("appserverPort", url.port(DEFAULT_APP_SERVER_PORT));
}

void QnSystrayWindow::onShowMediaServerLogAction()
{
    QString logFileName = m_mServerSettings.value("logFile").toString() + QString(".log");
    QProcess::startDetached(QString("notepad ") + logFileName);
};

void QnSystrayWindow::onShowAppServerLogAction()
{
    QString logFileName = m_mServerSettings.value("logFile").toString();
    logFileName = logFileName.left(logFileName.lastIndexOf(MEDIA_SERVER_NAME));
    logFileName += APP_SERVER_NAME;
    logFileName += "\\appserver.log";
    QProcess::startDetached(QString("notepad ") + logFileName);
}

void QnSystrayWindow::onSettingsAction()
{
    QUrl appServerUrl = getAppServerURL();

    QStringList urlList = m_settings.value("appserverUrlHistory").toString().split(';');
    urlList.insert(0, appServerUrl.toString());
    urlList.removeDuplicates();
    
    ui->appServerUrlComboBox->clear();
    foreach(const QString& value, urlList) {
        ui->appServerUrlComboBox->addItem(value);
    }

    ui->appServerLogin->setText(m_mServerSettings.value("appserverLogin").toString());
    ui->appServerPassword->setText(m_mServerSettings.value("appserverPassword").toString());
    ui->rtspPortLineEdit->setText(m_mServerSettings.value("rtspPort").toString());
    ui->apiPortLineEdit->setText(m_mServerSettings.value("apiPort").toString());
    ui->appServerPortLineEdit->setText(m_appServerSettings.value("port").toString());

    showNormal();
}

bool QnSystrayWindow::isAppServerParamChanged() const
{
    return ui->appServerPortLineEdit->text().toInt() != m_appServerSettings.value("port").toInt();
}

bool QnSystrayWindow::isMediaServerParamChanged() const
{
    QUrl savedURL = getAppServerURL();
    QUrl currentURL(ui->appServerUrlComboBox->currentText());
    if (savedURL.host() != currentURL.host() || savedURL.port(DEFAULT_APP_SERVER_PORT) != currentURL.port(DEFAULT_APP_SERVER_PORT))
        return true;


    if (ui->appServerLogin->text() != m_mServerSettings.value("appserverLogin").toString())
        return true;

    if (ui->appServerPassword->text() != m_mServerSettings.value("appserverPassword").toString())
        return true;

    if (ui->rtspPortLineEdit->text().toInt() != m_mServerSettings.value("rtspPort").toInt())
        return true;

    if (ui->apiPortLineEdit->text().toInt() != m_mServerSettings.value("apiPort").toInt())
        return true;

    return false;
}

void QnSystrayWindow::buttonClicked(QAbstractButton * button)
{
    if (ui->buttonBox->standardButton(button) == QDialogButtonBox::Ok)
    {
        if (validateData()) 
        {
            bool appServerParamChanged = isAppServerParamChanged();
            bool mediaServerParamChanged = isMediaServerParamChanged();
            saveData();
            if (appServerParamChanged || mediaServerParamChanged)
            {
                QString requestStr = tr("The changes you made require ");
                if (appServerParamChanged)
                    requestStr += APP_SERVER_NAME;
                if (mediaServerParamChanged) {
                    if (appServerParamChanged)
                        requestStr += QString(' ') + tr("and") + QString(' ');
                    requestStr += MEDIA_SERVER_NAME;
                }

                requestStr += tr(" to be restarted. Would you like to restart now?");
                if (QMessageBox::question(this, tr("Systray"), requestStr, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
                {
                    SERVICE_STATUS serviceStatus;
                    if (appServerParamChanged) {
                        ControlService(m_appServerHandle, SERVICE_CONTROL_STOP, &serviceStatus);
                        m_needStartAppServer = true;
                        m_waitingAppServerStarted = true;
                    }
                    if (mediaServerParamChanged) {
                        ControlService(m_mediaServerHandle, SERVICE_CONTROL_STOP, &serviceStatus);
                        m_needStartMediaServer = true;
                        m_waitingMediaServerStarted = true;
                    }
                    updateServiceInfo();
                    // restart services
                }
            }
        }
        else {
            QTimer::singleShot(0, this, SLOT(showNormal()));
        }
    }
}

bool QnSystrayWindow::checkPort(const QString& text, const QString& message)
{
    int port = text.toInt();
    if (port < 1 || port > 65535)
    {
        QMessageBox::warning(this, tr("Systray"), message);
        return false;
    }
    return true;
}

bool QnSystrayWindow::validateData()
{
    if (!checkPort(ui->appServerPortLineEdit->text(), "Invalid application server port specified."))
        return false;
    if (!checkPort(ui->rtspPortLineEdit->text(), "Invalid media server RTSP port specified."))
        return false;
    if (!checkPort(ui->apiPortLineEdit->text(), "Invalid media server API port specified."))
        return false;
    return true;
}

void QnSystrayWindow::saveData()
{
    m_mServerSettings.setValue("appserverLogin", ui->appServerLogin->text());
    m_mServerSettings.setValue("appserverPassword",ui->appServerPassword->text());
    m_mServerSettings.setValue("rtspPort", ui->rtspPortLineEdit->text());
    m_mServerSettings.setValue("apiPort", ui->apiPortLineEdit->text());
    m_appServerSettings.setValue("port", ui->appServerPortLineEdit->text());

    QStringList urlList = m_settings.value("appserverUrlHistory").toString().split(';');
    urlList.insert(0, getAppServerURL().toString());
    urlList.removeDuplicates();
    QString rez;
    foreach(QString str, urlList)
    {
        str = str.trimmed();
        if (!str.isEmpty()) {
            if (!rez.isEmpty())
                rez += ';';
            rez += str;
        }
    }
    m_settings.setValue("appserverUrlHistory", rez);
    setAppServerURL(ui->appServerUrlComboBox->currentText());
}

void QnSystrayWindow::onTestButtonClicked()
{
    QUrl url = ui->appServerUrlComboBox->currentText();
    url.setUserName(ui->appServerLogin->text());
    url.setPassword(ui->appServerPassword->text());

    if (!url.isValid())
    {
        QMessageBox::warning(this, tr("Invalid paramters"), tr("You have entered invalid URL."));
        return;
    }

    ConnectionTestingDialog dialog(this, url);
    dialog.setModal(true);
    dialog.exec();
}
