#include <QtGui>
#include <QTcpServer>

#include <utils/network/foundenterprisecontrollersmodel.h>

#include "systraywindow.h"
#include "ui_settings.h"
#include "ui_findappserverdialog.h"
#include "connection_testing_dialog.h"

#include <shlobj.h>
#include "version.h"

#pragma comment(lib, "Shell32.lib") /* For IsUserAnAdmin. */
#pragma comment(lib, "AdvApi32.lib") /* For ControlService and other service-related functions. */


#define USE_SINGLE_STREAMING_PORT

static const QString MEDIA_SERVER_NAME (QString(QN_ORGANIZATION_NAME) + QString(" Media Server"));
static const QString APP_SERVER_NAME("Enterprise Controller");
static const int DEFAULT_APP_SERVER_PORT = 8000;
static const int MESSAGE_DURATION = 3 * 1000;
static const int DEFAUT_PROXY_PORT = 7009;

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

QnSystrayWindow::QnSystrayWindow( FoundEnterpriseControllersModel* const foundEnterpriseControllersModel )
:
    ui(new Ui::SettingsDialog),
    m_findAppServerDialog(new QDialog()),
    m_findAppServerDialogUI( new Ui::FindAppServerDialog() ),
    m_mServerSettings(QSettings::SystemScope, qApp->organizationName(), MEDIA_SERVER_NAME),
    m_appServerSettings(QSettings::SystemScope, qApp->organizationName(), APP_SERVER_NAME),
    m_foundEnterpriseControllersModel( foundEnterpriseControllersModel )
{
    ui->setupUi(this);
    m_findAppServerDialogUI->setupUi(m_findAppServerDialog.data());
    m_findAppServerDialogUI->foundEnterpriseControllersView->setModel( m_foundEnterpriseControllersModel );

    connect(
        m_findAppServerDialogUI->foundEnterpriseControllersView,
        SIGNAL(doubleClicked(const QModelIndex&)),
        m_findAppServerDialog.data(),
        SLOT(accept()) );

#ifdef USE_SINGLE_STREAMING_PORT
    ui->apiPortLineEdit->setVisible(false);
    ui->label_ApiPort->setVisible(false);
    ui->label_RtspPort->setText(tr("Port"));
#endif

    m_iconOK = QIcon(":/traytool.png");
    m_iconBad = QIcon(":/traytool.png");

    m_mediaServerHandle = 0;
    m_appServerHandle = 0;
    m_skipTicks = 0;

    m_mediaServerServiceName = QString(QN_CUSTOMIZATION_NAME) + QString("MediaServer");
    m_appServerServiceName = QString(QN_CUSTOMIZATION_NAME) + QString("AppServer");

    m_mediaServerStartAction = 0;
    m_mediaServerStopAction = 0;
    m_appServerStartAction = 0;
    m_appServerStopAction = 0;
    m_scManager = 0;
    m_firstTimeToolTipError = true;
    m_needStartMediaServer = false;
    m_needStartAppServer = false;
    
    m_prevMediaServerStatus = -1;
    m_prevAppServerStatus = -1;
    m_lastMessageTimer.restart();

    findServiceInfo();

    createActions();
    createTrayIcon();


    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton * )), this, SLOT(buttonClicked(QAbstractButton * )));

    connect(trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    connect(ui->testButton, SIGNAL(clicked()), this, SLOT(onTestButtonClicked()));

    trayIcon->show();

    setWindowTitle(tr("VMS settings"));

    connect(&m_findServices, SIGNAL(timeout()), this, SLOT(findServiceInfo()));
    connect(&m_updateServiceStatus, SIGNAL(timeout()), this, SLOT(updateServiceInfo()));

    connect(ui->findAppServerButton, SIGNAL(clicked()), this, SLOT(onFindAppServerButtonClicked()));

    connect(ui->appServerUrlComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onAppServerUrlHistoryComboBoxCurrentChanged(int)));

    connect(ui->radioButtonPublicIPAuto, SIGNAL(toggled (bool)), this, SLOT(onRadioButtonPublicIpChanged()));
    connect(ui->radioButtonCustomPublicIP, SIGNAL(toggled (bool)), this, SLOT(onRadioButtonPublicIpChanged()));

    m_findServices.start(10000);
    m_updateServiceStatus.start(500);

    ui->informationLabel->setText(
        tr(
            "<b>%1</b> version %2 (%3).<br/>\n"
            "Engine version %4.<br/>\n"
            "Built for %5-%6 with %7.<br/>\n"
        ).
        arg(QLatin1String(QN_APPLICATION_NAME)).
        arg(QLatin1String(QN_APPLICATION_VERSION)).
        arg(QLatin1String(QN_APPLICATION_REVISION)).
        arg(QLatin1String(QN_ENGINE_VERSION)).
        arg(QLatin1String(QN_APPLICATION_PLATFORM)).
        arg(QLatin1String(QN_APPLICATION_ARCH)).
        arg(QLatin1String(QN_APPLICATION_COMPILER))
    );

    connect(ui->appServerPassword, SIGNAL(textChanged(const QString &)), this, SLOT(at_appServerPassword_textChanged(const QString &)));
}

void QnSystrayWindow::handleMessage(const QString& message)
{
    if (message == "quit") {
        qApp->quit();
    } else if (message == "activate") {
        return;
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
        m_mediaServerHandle = OpenService(m_scManager, (LPCWSTR) m_mediaServerServiceName.data(), SERVICE_ALL_ACCESS);
    if (m_mediaServerHandle == 0)
        m_mediaServerHandle = OpenService(m_scManager, (LPCWSTR) m_mediaServerServiceName.data(), SERVICE_QUERY_STATUS);

    if (m_appServerHandle == 0)
        m_appServerHandle  = OpenService(m_scManager, (LPCWSTR) m_appServerServiceName.data(),   SERVICE_ALL_ACCESS);
    if (m_appServerHandle == 0)
        m_appServerHandle  = OpenService(m_scManager, (LPCWSTR) m_appServerServiceName.data(),   SERVICE_QUERY_STATUS);

    if (!m_mediaServerHandle && !m_appServerHandle)
        showMessage(QString("No %1 services installed").arg(QN_ORGANIZATION_NAME));
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

void QnSystrayWindow::setIcon(int /*index*/)
{
    //QIcon icon = iconComboBox->itemIcon(index);
    //trayIcon->setIcon(icon);
    //setWindowIcon(icon);

    //trayIcon->setToolTip(iconComboBox->itemText(index));
}


void QnSystrayWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::DoubleClick:
        case QSystemTrayIcon::Trigger:
            trayIcon->contextMenu()->popup(QCursor::pos());
            break;
        case QSystemTrayIcon::MiddleClick:
            break;
        default:
            break;
    }
}

void QnSystrayWindow::showMessage(const QString& message)
{
    if (m_lastMessageTimer.elapsed() >= 1000) {
        trayIcon->showMessage(message, message, QSystemTrayIcon::Information, MESSAGE_DURATION);
        m_lastMessageTimer.restart();
    }
    else {
        m_delayedMessages << message;
        QTimer::singleShot(1500, this, SLOT(onDelayedMessage()));
    }
}

void QnSystrayWindow::onRadioButtonPublicIpChanged()
{
    ui->staticPublicIPEdit->setEnabled(ui->radioButtonCustomPublicIP->isChecked());
}

void QnSystrayWindow::onDelayedMessage()
{
    if (!m_delayedMessages.isEmpty())
    {
        QString message = m_delayedMessages.dequeue();
        showMessage(message);
    }
}

void QnSystrayWindow::messageClicked()
{
      
}

void QnSystrayWindow::updateServiceInfo()
{
    if (m_skipTicks > 0) {
        m_skipTicks--;
        return;
    }

    GetServiceInfoAsyncTask *mediaServerTask = new GetServiceInfoAsyncTask(m_mediaServerHandle);
    connect(mediaServerTask, SIGNAL(finished(quint64)), this, SLOT(mediaServerInfoUpdated(quint64)), Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(mediaServerTask);
   
    GetServiceInfoAsyncTask *appServerTask = new GetServiceInfoAsyncTask(m_appServerHandle);
    connect(appServerTask, SIGNAL(finished(quint64)), this, SLOT(appServerInfoUpdated(quint64)), Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(appServerTask);
}

void QnSystrayWindow::appServerInfoUpdated(quint64 status) {
    updateServiceInfoInternal(m_appServerHandle, status,  APP_SERVER_NAME, m_appServerStartAction,   m_appServerStopAction, m_showAppLogAction);
    settingsAction->setVisible(true);
    if (status == SERVICE_STOPPED) 
    {
        if (m_needStartAppServer)
        {
            m_needStartAppServer = false;
            StartService(m_appServerHandle, NULL, 0);
        }

        if (m_prevAppServerStatus >= 0 && m_prevAppServerStatus != SERVICE_STOPPED && !m_needStartAppServer)
        {
            showMessage(APP_SERVER_NAME + QString(" has been stopped"));
        }
    }
    else if (status == SERVICE_RUNNING)
    {
        if (m_prevAppServerStatus >= 0 && m_prevAppServerStatus != SERVICE_RUNNING)
        {
            showMessage(APP_SERVER_NAME + QString(" has been started"));
        }
    }
    m_prevAppServerStatus = status;
}

void QnSystrayWindow::mediaServerInfoUpdated(quint64 status) {
    updateServiceInfoInternal(m_mediaServerHandle, status, MEDIA_SERVER_NAME, m_mediaServerStartAction, m_mediaServerStopAction, m_showMediaServerLogAction);
    settingsAction->setVisible(true);
    if (status == SERVICE_STOPPED)
    {
        if (m_needStartMediaServer) 
        {
            m_needStartMediaServer = false;
            StartService(m_mediaServerHandle, NULL, 0);
        }

        if (m_prevMediaServerStatus >= 0 && m_prevMediaServerStatus != SERVICE_STOPPED && !m_needStartMediaServer)
        {
            showMessage(MEDIA_SERVER_NAME + QString(" has been stopped"));
        }
    }
    else if (status == SERVICE_RUNNING)
    {
        if (m_prevMediaServerStatus >= 0 && m_prevMediaServerStatus != SERVICE_RUNNING)
        {
            showMessage(MEDIA_SERVER_NAME + QString(" has been started"));
        }
    }
    m_prevMediaServerStatus = status;
}

void QnSystrayWindow::updateServiceInfoInternal(SC_HANDLE service, DWORD status, const QString& serviceName, QAction* startAction, QAction* stopAction, QAction* logAction)
{
    if (!service)
    {
        stopAction->setVisible(false);
        startAction->setVisible(false);
        logAction->setVisible(false);
        return;
    }
    logAction->setVisible(true);

    switch(status) {
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

void QnSystrayWindow::at_mediaServerStartAction()
{
    if (m_mediaServerHandle) {
        StartService(m_mediaServerHandle, NULL, 0);
        updateServiceInfo();
    }
}

void QnSystrayWindow::at_mediaServerStopAction()
{
    //SERVICE_STATUS serviceStatus;
    if (m_mediaServerHandle) 
    {
        if (QMessageBox::question(0, tr("Systray"), MEDIA_SERVER_NAME + " is going to be stopped. Are you sure?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            //ControlService(m_mediaServerHandle, SERVICE_CONTROL_STOP, &serviceStatus);
            StopServiceAsyncTask *stopTask = new StopServiceAsyncTask(m_mediaServerHandle);
            connect(stopTask, SIGNAL(finished()), this, SLOT(updateServiceInfo()), Qt::QueuedConnection);
            QThreadPool::globalInstance()->start(stopTask);
            m_mediaServerStopAction->setEnabled(false);
        }
    }
}

void QnSystrayWindow::at_appServerStartAction()
{
    if (m_appServerHandle) {
        StartService(m_appServerHandle, NULL, 0);
    }
}

void QnSystrayWindow::at_appServerStopAction()
{
    if (m_appServerHandle) 
    {
        if (QMessageBox::question(0, tr("Systray"), APP_SERVER_NAME + QString(" is going to be stopped. Are you sure?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            StopServiceAsyncTask *stopTask = new StopServiceAsyncTask(m_appServerHandle);
            connect(stopTask, SIGNAL(finished()), this, SLOT(updateServiceInfo()), Qt::QueuedConnection);
            QThreadPool::globalInstance()->start(stopTask);
            m_appServerStartAction->setEnabled(false);
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
    settingsAction->setVisible(m_mediaServerHandle || m_appServerHandle);

    m_showMediaServerLogAction = new QAction(tr("&Show %1 log").arg(MEDIA_SERVER_NAME), this);
    m_actionList.append(NameAndAction("showMediaServerLog", m_showMediaServerLogAction));
    connectElevatedAction(m_showMediaServerLogAction, SIGNAL(triggered()), this, SLOT(onShowMediaServerLogAction()));

    m_showAppLogAction = new QAction(tr("&Show %1 log").arg(APP_SERVER_NAME), this);
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
    appServerUrl.setScheme("https");
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
    QString logFileName = m_appServerSettings.value("logFile").toString();
    QProcess::startDetached(QString("notepad ") + logFileName);
}

void QnSystrayWindow::onSettingsAction()
{
    QUrl appServerUrl = getAppServerURL();

    QStringList urlList = m_settings.value("appserverUrlHistory").toString().split(';');
    urlList.insert(0, appServerUrl.toString());
    urlList.removeDuplicates();

    ui->appServerUrlComboBox->clear();
    ui->appServerUrlComboBox->addItem( tr("* Last used connection *") );
    foreach(const QString& value, urlList) {
        ui->appServerUrlComboBox->addItem(value);
    }

    ui->appIPEdit->setText(m_mServerSettings.value("appserverHost").toString());
    ui->appPortSpinBox->setValue(m_mServerSettings.value("appserverPort").toInt());
    ui->appServerLogin->setText(m_mServerSettings.value("appserverLogin").toString());
    ui->appServerPassword->setText(m_mServerSettings.value("appserverPassword").toString());
    ui->rtspPortLineEdit->setText(m_mServerSettings.value("rtspPort").toString());
    
    ui->staticPublicIPEdit->setText(m_mServerSettings.value("staticPublicIP").toString());
    if (m_mServerSettings.value("publicIPEnabled").isNull())
        m_mServerSettings.setValue("publicIPEnabled", 1);
    int allowPublicIP = m_mServerSettings.value("publicIPEnabled").toInt();
    ui->groupBoxPublicIP->setChecked(allowPublicIP > 0);
    ui->radioButtonPublicIPAuto->setChecked(allowPublicIP < 1);
    ui->radioButtonCustomPublicIP->setChecked(allowPublicIP > 1);
    onRadioButtonPublicIpChanged();

    QString rtspTransport = m_mServerSettings.value("rtspTransport", "AUTO").toString().toUpper();
    if (rtspTransport == "UDP")
        ui->rtspTransportComboBox->setCurrentIndex(1);
    else if (rtspTransport == "TCP")
        ui->rtspTransportComboBox->setCurrentIndex(2);
    else
        ui->rtspTransportComboBox->setCurrentIndex(0);

#ifndef USE_SINGLE_STREAMING_PORT
    ui->apiPortLineEdit->setText(m_mServerSettings.value("apiPort").toString());
#endif
    ui->appServerPortLineEdit->setText(m_appServerSettings.value("port").toString());
    ui->proxyPortLineEdit->setText(m_appServerSettings.value("proxyPort", DEFAUT_PROXY_PORT).toString());

    ui->tabAppServer->setEnabled(m_appServerHandle != 0);
    ui->tabMediaServer->setEnabled(m_mediaServerHandle != 0);

    showNormal();
}

bool QnSystrayWindow::isAppServerParamChanged() const
{
    return ui->appServerPortLineEdit->text().toInt() != m_appServerSettings.value("port").toInt() ||
           ui->proxyPortLineEdit->text().toInt() != m_appServerSettings.value("proxyPort", DEFAUT_PROXY_PORT).toInt();
}

bool QnSystrayWindow::isMediaServerParamChanged() const
{
    QUrl savedURL = getAppServerURL();
    QUrl currentURL( QString::fromAscii("https://%1:%2").arg(ui->appIPEdit->text()).arg(ui->appPortSpinBox->value()) );
    if (savedURL.host() != currentURL.host() || savedURL.port(DEFAULT_APP_SERVER_PORT) != currentURL.port(DEFAULT_APP_SERVER_PORT))
        return true;

    if (ui->appServerLogin->text() != m_mServerSettings.value("appserverLogin").toString())
        return true;

    if (ui->appServerPassword->text() != m_mServerSettings.value("appserverPassword").toString())
        return true;

    if (ui->rtspPortLineEdit->text().toInt() != m_mServerSettings.value("rtspPort").toInt())
        return true;

#ifndef USE_SINGLE_STREAMING_PORT
    if (ui->apiPortLineEdit->text().toInt() != m_mServerSettings.value("apiPort").toInt())
        return true;
#endif

    if (m_mServerSettings.value("staticPublicIP").toString().trimmed() != ui->staticPublicIPEdit->text().trimmed() && ui->radioButtonCustomPublicIP->isChecked())
        return true;

    int publicIPState;
    if (!ui->groupBoxPublicIP->isChecked())
        publicIPState = 0;
    else if (ui->radioButtonPublicIPAuto->isChecked())
        publicIPState = 1;
    else
        publicIPState = 2;
    if (m_mServerSettings.value("publicIPEnabled").toInt() != publicIPState)
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
                // TODO: #VASILENKO Untranslatable strings.

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
                    //SERVICE_STATUS serviceStatus;
                    if (appServerParamChanged) {
                        //ControlService(m_appServerHandle, SERVICE_CONTROL_STOP, &serviceStatus);
                        QThreadPool::globalInstance()->start(new StopServiceAsyncTask(m_appServerHandle));

                        m_needStartAppServer = true;
                        m_prevAppServerStatus = SERVICE_STOPPED;

                    }
                    if (mediaServerParamChanged) {
                        //ControlService(m_mediaServerHandle, SERVICE_CONTROL_STOP, &serviceStatus);
                        QThreadPool::globalInstance()->start(new StopServiceAsyncTask(m_mediaServerHandle));

                        m_needStartMediaServer = true;
                        m_prevMediaServerStatus = SERVICE_STOPPED;
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

bool QnSystrayWindow::checkPortNum(int port, const QString& message)
{
    if (port < 1 || port > 65535)
    {
        QMessageBox::warning(this, tr("Systray"), QString("Invalid ") + message + QString(" port specified."));
        return false;
    }
    return true;
}

bool QnSystrayWindow::isPortFree(int port)
{
    QTcpServer testServer;
    return testServer.listen(QHostAddress::Any, port);
}

bool QnSystrayWindow::validateData()
{
    struct PortInfo
    {
        PortInfo(int _newPort, int _oldPort, const QString& _descriptor, int _tabIndex)
        {
            newPort = _newPort;
            descriptor = _descriptor;
            oldPort = _oldPort;
            tabIndex = _tabIndex;
        }

        int newPort;
        int oldPort;
        QString descriptor;
        int tabIndex;
    };
    QList<PortInfo> checkedPorts;

    if (m_appServerHandle)
    {
        checkedPorts << PortInfo(ui->appServerPortLineEdit->text().toInt(), m_appServerSettings.value("port").toInt(), APP_SERVER_NAME, 1);
        checkedPorts << PortInfo(ui->proxyPortLineEdit->text().toInt(), m_appServerSettings.value("proxyPort", DEFAUT_PROXY_PORT).toInt(), "media proxy", 1);
    }

    if (m_mediaServerHandle)
    {
        checkedPorts << PortInfo(ui->rtspPortLineEdit->text().toInt(), m_mServerSettings.value("rtspPort").toInt(), "media server RTSP", 0);
#ifndef USE_SINGLE_STREAMING_PORT
        checkedPorts << PortInfo(ui->apiPortLineEdit->text().toInt(), m_mServerSettings.value("apiPort").toInt(), "media server API", 0);
#endif
    }
    
    for(int i = 0; i < checkedPorts.size(); ++i)
    {
        if (!checkPortNum(checkedPorts[i].newPort, checkedPorts[i].descriptor))
        {
            if( ui->tabWidget->currentIndex() != checkedPorts[i].tabIndex )
                ui->tabWidget->setCurrentIndex( checkedPorts[i].tabIndex );
            return false;
        }
        for (int j = i+1; j < checkedPorts.size(); ++j) {
            if (checkedPorts[i].newPort == checkedPorts[j].newPort)
            {
                QMessageBox::warning(this, tr("Systray"), checkedPorts[i].descriptor + QString(" port is same as ") + checkedPorts[j].descriptor + QString(" port"));
                return false;
            }
        }
        bool isNewPort = true;
        for (int j = 0; j < checkedPorts.size(); ++j) {
            if (checkedPorts[j].oldPort == checkedPorts[i].newPort)
                isNewPort = false;
        }
        if (isNewPort && !isPortFree(checkedPorts[i].newPort)) {
            QMessageBox::warning(this, tr("Systray"), checkedPorts[i].descriptor + QString(" port already used by another process"));
            return false;
        }
    }
    QString staticPublicIP = ui->staticPublicIPEdit->text().trimmed();
    if (!staticPublicIP.isEmpty() && QHostAddress(staticPublicIP).isNull())
    {
        QMessageBox::warning(this, tr("Systray"), QString(" Invalid IP address specified for public IP address"));
        return false;
    }

    return true;
}

void QnSystrayWindow::saveData()
{
    m_mServerSettings.setValue("appserverLogin", ui->appServerLogin->text());
    m_mServerSettings.setValue("appserverPassword",ui->appServerPassword->text());
    m_mServerSettings.setValue("rtspPort", ui->rtspPortLineEdit->text());
#ifndef USE_SINGLE_STREAMING_PORT
    m_mServerSettings.setValue("apiPort", ui->apiPortLineEdit->text());
#endif

    m_mServerSettings.setValue("rtspTransport", ui->rtspTransportComboBox->currentText());

    m_appServerSettings.setValue("port", ui->appServerPortLineEdit->text());
    m_appServerSettings.setValue("proxyPort", ui->proxyPortLineEdit->text());

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

    setAppServerURL( QString::fromAscii("https://%1:%2").arg(ui->appIPEdit->text()).arg(ui->appPortSpinBox->value()) );


    m_mServerSettings.setValue("staticPublicIP", ui->staticPublicIPEdit->text());
    if (!ui->groupBoxPublicIP->isChecked())
        m_mServerSettings.setValue("publicIPEnabled", 0);
    else if (ui->radioButtonPublicIPAuto->isChecked())
        m_mServerSettings.setValue("publicIPEnabled", 1);
    else
        m_mServerSettings.setValue("publicIPEnabled", 2);
}

void QnSystrayWindow::onTestButtonClicked()
{
    QUrl url( QString::fromAscii("https://%1:%2").arg(ui->appIPEdit->text()).arg(ui->appPortSpinBox->value()) );

    url.setUserName(ui->appServerLogin->text());
    url.setPassword(ui->appServerPassword->text());

    if (!url.isValid())
    {
        QMessageBox::warning(this, tr("Invalid parameters"), tr("You have entered invalid URL."));
        return;
    }

    QnConnectionTestingDialog dialog(url, this);
    dialog.setModal(true);
    dialog.exec();
}

void QnSystrayWindow::onFindAppServerButtonClicked()
{
    if( !m_findAppServerDialog->exec() )
        return; //cancel pressed
    const QModelIndex& selectedSrvIndex = m_findAppServerDialogUI->foundEnterpriseControllersView->currentIndex();
    if( !selectedSrvIndex.isValid() )
        return;

    ui->appIPEdit->setText( m_foundEnterpriseControllersModel->data(selectedSrvIndex, FoundEnterpriseControllersModel::appServerIPRole).toString() );
    ui->appPortSpinBox->setValue( m_foundEnterpriseControllersModel->data(selectedSrvIndex, FoundEnterpriseControllersModel::appServerPortRole).toInt() );
}

void QnSystrayWindow::onAppServerUrlHistoryComboBoxCurrentChanged( int index )
{
    if( index == 0 )
        return; //selected "Last used connection" element

    //filling in edits with values
    const QUrl urlToSet( ui->appServerUrlComboBox->currentText() );
    ui->appIPEdit->setText( urlToSet.host() );
    ui->appPortSpinBox->setValue( urlToSet.port() );
    //ui->appServerLogin->setText( urlToSet.userName() );
    //ui->appServerPassword->setText( urlToSet.password() );
}
void QnSystrayWindow::at_appServerPassword_textChanged(const QString &text) {
    ui->testButton->setEnabled(!text.isEmpty());
}