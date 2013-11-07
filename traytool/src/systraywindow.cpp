#include <QtCore/QSettings>
#include <QtCore/QThreadPool>
#include <QtCore/QProcess>
#include <QtNetwork/QTcpServer>
#include <QtWidgets/QMenu>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QMessageBox>

#include <utils/network/foundenterprisecontrollersmodel.h>

#include <translation/translation_manager.h>
#include <translation/translation_list_model.h>

#include "systraywindow.h"
#include "ui_settings.h"
#include "ui_findappserverdialog.h"
#include "connection_testing_dialog.h"
#include "common/common_module.h"

#include <shlobj.h>
#include "version.h"

#pragma comment(lib, "Shell32.lib") /* For IsUserAnAdmin. */
#pragma comment(lib, "AdvApi32.lib") /* For ControlService and other handle-related functions. */


#define USE_SINGLE_STREAMING_PORT

static const QString MEDIA_SERVER_NAME(QString(lit(QN_ORGANIZATION_NAME)) + lit(" Media Server"));
static const QString APP_SERVER_NAME(lit("Enterprise Controller"));
static const QString CLIENT_NAME(QString(lit(QN_ORGANIZATION_NAME)) + lit(" ") + lit(QN_PRODUCT_NAME) + lit(" Client"));

static const int DEFAULT_APP_SERVER_PORT = 8000;
static const int MESSAGE_DURATION = 3 * 1000;
static const int DEFAUT_PROXY_PORT = 7009;

static const QString ECS_PUBLIC_IP_MODE_AUTO = lit("auto");
static const QString ECS_PUBLIC_IP_MODE_MANUAL = lit("manual");
static const QString ECS_PUBLIC_IP_MODE_DISABLED = lit("disabled");

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

QnSystrayWindow::QnSystrayWindow(FoundEnterpriseControllersModel *const foundEnterpriseControllersModel):
    ui(new Ui::SettingsDialog),
    m_findAppServerDialog(new QDialog()),
    m_findAppServerDialogUI(new Ui::FindAppServerDialog()),
    m_mediaServerSettings(QSettings::SystemScope, qApp->organizationName(), MEDIA_SERVER_NAME),
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

    m_iconOK = QIcon(lit(":/traytool.png"));
    m_iconBad = QIcon(lit(":/traytool.png"));

    m_mediaServerHandle = 0;
    m_appServerHandle = 0;
    m_skipTicks = 0;

    m_mediaServerServiceName = QString(lit(QN_CUSTOMIZATION_NAME)) + lit("MediaServer");
    m_appServerServiceName = QString(lit(QN_CUSTOMIZATION_NAME)) + lit("AppServer");

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

    connect(m_trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
    connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    connect(ui->testButton, SIGNAL(clicked()), this, SLOT(onTestButtonClicked()));

    m_trayIcon->show();

    setWindowTitle(QString(lit(QN_ORGANIZATION_NAME)) + tr(" Tray Assistant"));

    connect(&m_findServices, SIGNAL(timeout()), this, SLOT(findServiceInfo()));
    connect(&m_updateServiceStatus, SIGNAL(timeout()), this, SLOT(updateServiceInfo()));

    connect(ui->findAppServerButton, SIGNAL(clicked()), this, SLOT(onFindAppServerButtonClicked()));

    connect(ui->appServerUrlComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onAppServerUrlHistoryComboBoxCurrentChanged(int)));

    connect(ui->radioButtonPublicIPAuto, SIGNAL(toggled (bool)), this, SLOT(onRadioButtonPublicIpChanged()));
    connect(ui->radioButtonCustomPublicIP, SIGNAL(toggled (bool)), this, SLOT(onRadioButtonPublicIpChanged()));

    connect(ui->ecsUseAutoPublicIp, SIGNAL(toggled (bool)), this, SLOT(onRadioButtonEcsPublicIpChanged()));
    connect(ui->ecsUseManualPublicIp, SIGNAL(toggled (bool)), this, SLOT(onRadioButtonEcsPublicIpChanged()));

    m_findServices.start(10000);
    m_updateServiceStatus.start(500);

    ui->informationLabel->setText(
        tr(
            "%1 Tray Assistant version %2 (%3).<br/>\n"
            "Built for %5-%6 with %7.<br/>\n"
        ).
        arg(QLatin1String(QN_PRODUCT_NAME)).
        arg(QLatin1String(QN_APPLICATION_VERSION)).
        arg(QLatin1String(QN_APPLICATION_REVISION)).
        arg(QLatin1String(QN_APPLICATION_PLATFORM)).
        arg(QLatin1String(QN_APPLICATION_ARCH)).
        arg(QLatin1String(QN_APPLICATION_COMPILER))
    );

    connect(ui->appServerPassword, SIGNAL(textChanged(const QString &)), this, SLOT(at_appServerPassword_textChanged(const QString &)));

    initTranslations();

    m_mediaServerStartAction->setVisible(false);
    m_mediaServerStopAction->setVisible(false);
    m_showMediaServerLogAction->setVisible(false);
    m_appServerStartAction->setVisible(false);
    m_appServerStopAction->setVisible(false);
    m_showAppLogAction->setVisible(false);
}

void QnSystrayWindow::initTranslations() {
    QnTranslationManager *translationManager = qnCommon->instance<QnTranslationManager>();

    QnTranslationListModel *model = new QnTranslationListModel(this);
    model->setTranslations(translationManager->loadTranslations());
    ui->languageComboBox->setModel(model);

    // TODO: #Elric code duplication
    QSettings clientSettings(QSettings::UserScope, qApp->organizationName(), CLIENT_NAME);
    QString translationPath = clientSettings.value(lit("translationPath")).toString();
    int index = translationPath.lastIndexOf(lit("client"));
    if(index != -1)
        translationPath.replace(index, 6, lit("traytool"));

    for(int i = 0; i < ui->languageComboBox->count(); i++) {
        QnTranslation translation = ui->languageComboBox->itemData(i, Qn::TranslationRole).value<QnTranslation>();
        if(translation.filePaths().contains(translationPath)) {
            ui->languageComboBox->setCurrentIndex(i);
            break;
        }
    }
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
                m_detailedErrorText = tr("The requested access was denied");
                break;
            case ERROR_DATABASE_DOES_NOT_EXIST:
                m_detailedErrorText = tr("The specified database does not exist.");
                break;
            case ERROR_INVALID_PARAMETER:
                m_detailedErrorText =  tr("A specified parameter is invalid.");
                break;
        }
        if (m_firstTimeToolTipError) {
            m_trayIcon->showMessage(tr("Insufficient permissions to start/stop services"), m_detailedErrorText, QSystemTrayIcon::Critical, MESSAGE_DURATION);
            m_trayIcon->setIcon(m_iconBad);
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
        showMessage(tr("No %1 services installed").arg(lit(QN_ORGANIZATION_NAME)));
}

void QnSystrayWindow::setVisible(bool visible)
{
    //minimizeAction->setEnabled(visible);
    //maximizeAction->setEnabled(!isMaximized());
    m_settingsAction->setEnabled(isMaximized() || !visible);
    QDialog::setVisible(visible);
}

void QnSystrayWindow::accept() {
    base_type::accept();

    QnTranslation translation = ui->languageComboBox->itemData(ui->languageComboBox->currentIndex(), Qn::TranslationRole).value<QnTranslation>();
    if(!translation.isEmpty()) {
        // TODO: #Elric code duplication.
        QSettings clientSettings(QSettings::UserScope, qApp->organizationName(), CLIENT_NAME);
        
        QString translationPath;
        if(!translation.filePaths().isEmpty())
            translationPath = translation.filePaths()[0];
        int index = translationPath.lastIndexOf(lit("traytool"));
        if(index != -1)
            translationPath.replace(index, 8, lit("client"));

        clientSettings.setValue(lit("translationPath"), translationPath);
    }
}

void QnSystrayWindow::closeEvent(QCloseEvent *event)
{
    if (m_trayIcon->isVisible()) 
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
    //m_trayIcon->setIcon(icon);
    //setWindowIcon(icon);

    //m_trayIcon->setToolTip(iconComboBox->itemText(index));
}


void QnSystrayWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::DoubleClick:
        case QSystemTrayIcon::Trigger:
            m_trayIcon->contextMenu()->popup(QCursor::pos());
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
        m_trayIcon->showMessage(message, message, QSystemTrayIcon::Information, MESSAGE_DURATION);
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

void QnSystrayWindow::onRadioButtonEcsPublicIpChanged()
{
    ui->ecsManuaPublicIPEdit->setEnabled(ui->ecsUseManualPublicIp->isChecked());
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
    updateServiceInfoInternal(m_appServerHandle, status, m_appServerStartAction,   m_appServerStopAction, m_showAppLogAction);
    m_settingsAction->setVisible(true);
    if (status == SERVICE_STOPPED) 
    {
        if (m_needStartAppServer)
        {
            m_needStartAppServer = false;
            StartService(m_appServerHandle, NULL, 0);
        }

        if (m_prevAppServerStatus >= 0 && m_prevAppServerStatus != SERVICE_STOPPED && !m_needStartAppServer)
        {
            showMessage(tr("Enterprise controller has been stopped"));
        }
    }
    else if (status == SERVICE_RUNNING)
    {
        if (m_prevAppServerStatus >= 0 && m_prevAppServerStatus != SERVICE_RUNNING)
        {
            showMessage(tr("Enterprise controller has been started"));
        }
    }
    m_prevAppServerStatus = status;
}

void QnSystrayWindow::mediaServerInfoUpdated(quint64 status) {
    updateServiceInfoInternal(m_mediaServerHandle, status, m_mediaServerStartAction, m_mediaServerStopAction, m_showMediaServerLogAction);
    m_settingsAction->setVisible(true);
    if (status == SERVICE_STOPPED)
    {
        if (m_needStartMediaServer) 
        {
            m_needStartMediaServer = false;
            StartService(m_mediaServerHandle, NULL, 0);
        }

        if (m_prevMediaServerStatus >= 0 && m_prevMediaServerStatus != SERVICE_STOPPED && !m_needStartMediaServer)
        {
            showMessage(tr("Media server has been stopped"));
        }
    }
    else if (status == SERVICE_RUNNING)
    {
        if (m_prevMediaServerStatus >= 0 && m_prevMediaServerStatus != SERVICE_RUNNING)
        {
            showMessage(tr("Media server has been started"));
        }
    }
    m_prevMediaServerStatus = status;
}

void QnSystrayWindow::updateServiceInfoInternal(SC_HANDLE handle, DWORD status, QAction* startAction, QAction* stopAction, QAction* logAction)
{
    if (!handle) {
        stopAction->setVisible(false);
        startAction->setVisible(false);
        logAction->setVisible(false);
        return;
    }
    logAction->setVisible(true);

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
        if (QMessageBox::question(0, tr("Systray"), tr("Media server is going to be stopped. Are you sure?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            //ControlService(m_mediaServerHandle, SERVICE_CONTROL_STOP, &serviceStatus);
            StopServiceAsyncTask *stopTask = new StopServiceAsyncTask(m_mediaServerHandle);
            connect(stopTask, SIGNAL(finished()), this, SLOT(updateServiceInfo()), Qt::QueuedConnection);
            QThreadPool::globalInstance()->start(stopTask);
            updateServiceInfoInternal(m_mediaServerHandle, SERVICE_STOP_PENDING, m_mediaServerStartAction, m_mediaServerStopAction, m_showMediaServerLogAction);
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
        if (QMessageBox::question(0, tr("Systray"), tr("Enterprise controller is going to be stopped. Are you sure?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            StopServiceAsyncTask *stopTask = new StopServiceAsyncTask(m_appServerHandle);
            connect(stopTask, SIGNAL(finished()), this, SLOT(updateServiceInfo()), Qt::QueuedConnection);
            QThreadPool::globalInstance()->start(stopTask);
            updateServiceInfoInternal(m_appServerHandle, SERVICE_STOP_PENDING, m_appServerStartAction, m_appServerStopAction, m_showAppLogAction);
        }
    }
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
        systray->trayIcon()->showMessage(tr("Insufficient privileges to manage services"), tr("UAC must be enabled to request privileges for non-admin users"), QSystemTrayIcon::Warning, MESSAGE_DURATION);
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
    m_settingsAction = new QAction(tr("&Settings"), this);
    m_settingsAction->setObjectName(lit("showSettings"));
    m_actions.push_back(m_settingsAction);
    connectElevatedAction(m_settingsAction, SIGNAL(triggered()), this, SLOT(onSettingsAction()));
    m_settingsAction->setVisible(m_mediaServerHandle || m_appServerHandle);

    m_showMediaServerLogAction = new QAction(tr("&Show Media Server Log"), this);
    m_showMediaServerLogAction->setObjectName(lit("showMediaServerLog"));
    m_actions.push_back(m_showMediaServerLogAction);
    connectElevatedAction(m_showMediaServerLogAction, SIGNAL(triggered()), this, SLOT(onShowMediaServerLogAction()));

    m_showAppLogAction = new QAction(tr("&Show Enterprise Controller Log"), this);
    m_showAppLogAction->setObjectName(lit("showAppServerLog"));
    m_actions.push_back(m_showAppLogAction);
    connectElevatedAction(m_showAppLogAction, SIGNAL(triggered()), this, SLOT(onShowAppServerLogAction()));

    m_quitAction = new QAction(tr("&Quit"), this);
    connect(m_quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    m_mediaServerStartAction = new QAction(QString(tr("Start Media Server")), this);
    m_mediaServerStartAction->setObjectName(lit("startMediaServer"));
    m_actions.push_back(m_mediaServerStartAction);
    connectElevatedAction(m_mediaServerStartAction, SIGNAL(triggered()), this, SLOT(at_mediaServerStartAction()));

    m_mediaServerStopAction = new QAction(QString(tr("Stop Media Server")), this);
    m_mediaServerStopAction->setObjectName(lit("stopMediaServer"));
    m_actions.push_back(m_mediaServerStopAction);
    connectElevatedAction(m_mediaServerStopAction, SIGNAL(triggered()), this, SLOT(at_mediaServerStopAction()));

    m_appServerStartAction = new QAction(QString(tr("Start Enterprise Controller")), this);
    m_appServerStartAction->setObjectName(lit("startAppServer"));
    m_actions.push_back(m_appServerStartAction);
    connectElevatedAction(m_appServerStartAction, SIGNAL(triggered()), this, SLOT(at_appServerStartAction()));

    m_appServerStopAction = new QAction(QString(tr("Stop Enterprise Controller")), this);
    m_appServerStopAction->setObjectName(lit("stopAppServer"));
    m_actions.push_back(m_appServerStopAction);
    connectElevatedAction(m_appServerStopAction, SIGNAL(triggered()), this, SLOT(at_appServerStopAction()));

    updateServiceInfo();
}

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

void QnSystrayWindow::createTrayIcon()
{
    m_trayIconMenu = new QMenu();

    m_trayIconMenu->addAction(m_mediaServerStartAction);
    m_trayIconMenu->addAction(m_mediaServerStopAction);
    m_trayIconMenu->addAction(m_appServerStartAction);
    m_trayIconMenu->addAction(m_appServerStopAction);
    m_trayIconMenu->addSeparator();

    m_trayIconMenu->addAction(m_settingsAction);
    m_trayIconMenu->addSeparator();

    m_trayIconMenu->addAction(m_showMediaServerLogAction);
    m_trayIconMenu->addAction(m_showAppLogAction);
    m_trayIconMenu->addSeparator();

    m_trayIconMenu->addAction(m_quitAction);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayIconMenu);    
    m_trayIcon->setIcon(m_iconOK);
}

QUrl QnSystrayWindow::getAppServerURL() const
{
    QUrl appServerUrl;
    appServerUrl.setScheme(lit("https"));
    appServerUrl.setHost(m_mediaServerSettings.value(lit("appserverHost")).toString());
    int appServerPort = m_mediaServerSettings.value(lit("appserverPort")).toInt();
    appServerUrl.setPort(appServerPort != 0 ? appServerPort : DEFAULT_APP_SERVER_PORT);
    return appServerUrl;
}

void QnSystrayWindow::setAppServerURL(const QUrl& url)
{
    m_mediaServerSettings.setValue(lit("appserverHost"), url.host());
    m_mediaServerSettings.setValue(lit("appserverPort"), url.port(DEFAULT_APP_SERVER_PORT));
}

void QnSystrayWindow::onShowMediaServerLogAction()
{
    QString logFileName = m_mediaServerSettings.value(lit("logFile")).toString() + lit(".log");
    QProcess::startDetached(lit("notepad ") + logFileName);
};

void QnSystrayWindow::onShowAppServerLogAction()
{
    QString logFileName = m_appServerSettings.value(lit("logFile")).toString();
    QProcess::startDetached(lit("notepad ") + logFileName);
}

void QnSystrayWindow::onSettingsAction()
{
    QUrl appServerUrl = getAppServerURL();

    QStringList urlList = m_settings.value(lit("appserverUrlHistory")).toString().split(lit(';'));
    urlList.insert(0, appServerUrl.toString());
    urlList.removeDuplicates();

    ui->appServerUrlComboBox->clear();
    ui->appServerUrlComboBox->addItem(tr("* Last used connection *"));
    foreach(const QString& value, urlList)
        ui->appServerUrlComboBox->addItem(value);

    ui->appIPEdit->setText(m_mediaServerSettings.value(lit("appserverHost")).toString());
    ui->appPortSpinBox->setValue(m_mediaServerSettings.value(lit("appserverPort")).toInt());
    ui->appServerLogin->setText(m_mediaServerSettings.value(lit("appserverLogin")).toString());
    ui->appServerPassword->setText(m_mediaServerSettings.value(lit("appserverPassword")).toString());
    ui->rtspPortLineEdit->setText(m_mediaServerSettings.value(lit("rtspPort")).toString());
    
    ui->staticPublicIPEdit->setText(m_mediaServerSettings.value(lit("staticPublicIP")).toString());
    if (m_mediaServerSettings.value(lit("publicIPEnabled")).isNull())
        m_mediaServerSettings.setValue(lit("publicIPEnabled"), 1);
    int allowPublicIP = m_mediaServerSettings.value(lit("publicIPEnabled")).toInt();
    ui->groupBoxPublicIP->setChecked(allowPublicIP > 0);
    ui->radioButtonPublicIPAuto->setChecked(allowPublicIP < 1);
    ui->radioButtonCustomPublicIP->setChecked(allowPublicIP > 1);
    onRadioButtonPublicIpChanged();

    QString rtspTransport = m_mediaServerSettings.value(lit("rtspTransport"), lit("AUTO")).toString().toUpper();
    if (rtspTransport == lit("UDP"))
        ui->rtspTransportComboBox->setCurrentIndex(1);
    else if (rtspTransport == lit("TCP"))
        ui->rtspTransportComboBox->setCurrentIndex(2);
    else
        ui->rtspTransportComboBox->setCurrentIndex(0);

#ifndef USE_SINGLE_STREAMING_PORT
    ui->apiPortLineEdit->setText(m_mediaServerSettings.value(lit("apiPort")).toString());
#endif

    QString ecsPublicIpMode = m_appServerSettings.value(lit("publicIpMode")).toString();
    ui->ecsAllowPublicIpGroupBox->setChecked(ecsPublicIpMode != ECS_PUBLIC_IP_MODE_DISABLED);
    ui->ecsUseAutoPublicIp->setChecked(ecsPublicIpMode == ECS_PUBLIC_IP_MODE_AUTO);
    ui->ecsUseManualPublicIp->setChecked(ecsPublicIpMode == ECS_PUBLIC_IP_MODE_MANUAL);
    ui->ecsManuaPublicIPEdit->setText(m_appServerSettings.value(lit("manualPublicIp")).toString());

    onRadioButtonEcsPublicIpChanged();
    ui->ecsPortSpinBox->setValue(m_appServerSettings.value(lit("ecPort")).toInt());
    ui->mediaProxyPortSpinBox->setValue(m_appServerSettings.value(lit("port"), DEFAUT_PROXY_PORT).toInt());

    ui->tabAppServer->setEnabled(m_appServerHandle != 0);
    ui->tabMediaServer->setEnabled(m_mediaServerHandle != 0);

    showNormal();
}

bool QnSystrayWindow::isAppServerParamChanged() const
{
    if (ui->ecsPortSpinBox->value() != m_appServerSettings.value(lit("port")).toInt() ||
        ui->mediaProxyPortSpinBox->value() != m_appServerSettings.value(lit("proxyPort"), DEFAUT_PROXY_PORT).toInt())
        return true;

    if (m_appServerSettings.value(lit("manualPublicIp")).toString().trimmed() != ui->ecsManuaPublicIPEdit->text().trimmed() && ui->ecsUseManualPublicIp->isChecked())
        return true;

    QString publicIpMode;
    if (!ui->ecsAllowPublicIpGroupBox->isChecked())
        publicIpMode = ECS_PUBLIC_IP_MODE_DISABLED;
    else if (ui->ecsUseAutoPublicIp->isChecked())
        publicIpMode = ECS_PUBLIC_IP_MODE_AUTO;
    else
        publicIpMode= ECS_PUBLIC_IP_MODE_MANUAL;

    if (m_appServerSettings.value(lit("publicIpMode")).toString() != publicIpMode)
        return true;

    return false;
}

bool QnSystrayWindow::isMediaServerParamChanged() const
{
    QUrl savedURL = getAppServerURL();
    QUrl currentURL( QString::fromLatin1("https://%1:%2").arg(ui->appIPEdit->text()).arg(ui->appPortSpinBox->value()) );
    if (savedURL.host() != currentURL.host() || savedURL.port(DEFAULT_APP_SERVER_PORT) != currentURL.port(DEFAULT_APP_SERVER_PORT))
        return true;

    if (ui->appServerLogin->text() != m_mediaServerSettings.value(lit("appserverLogin")).toString())
        return true;

    if (ui->appServerPassword->text() != m_mediaServerSettings.value(lit("appserverPassword")).toString())
        return true;

    if (ui->rtspPortLineEdit->text().toInt() != m_mediaServerSettings.value(lit("rtspPort")).toInt())
        return true;

#ifndef USE_SINGLE_STREAMING_PORT
    if (ui->apiPortLineEdit->text().toInt() != m_mediaServerSettings.value(lit("apiPort")).toInt())
        return true;
#endif

    if (m_mediaServerSettings.value(lit("staticPublicIP")).toString().trimmed() != ui->staticPublicIPEdit->text().trimmed() && ui->radioButtonCustomPublicIP->isChecked())
        return true;

    int publicIPState;
    if (!ui->groupBoxPublicIP->isChecked())
        publicIPState = 0;
    else if (ui->radioButtonPublicIPAuto->isChecked())
        publicIPState = 1;
    else
        publicIPState = 2;
    if (m_mediaServerSettings.value(lit("publicIPEnabled")).toInt() != publicIPState)
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
                QString requestStr;
                if(appServerParamChanged && mediaServerParamChanged) {
                    requestStr = tr("The changes you made require enterprise controller and media server to be restarted. Would you like to restart now?");
                } else if(appServerParamChanged) {
                    requestStr = tr("The changes you made require enterprise controller to be restarted. Would you like to restart now?");
                } else {
                    requestStr = tr("The changes you made require media server to be restarted. Would you like to restart now?");
                }

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
        QMessageBox::warning(this, tr("Systray"), tr("Invalid %1 port specified.").arg(message));
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
        checkedPorts << PortInfo(ui->ecsPortSpinBox->value(), m_appServerSettings.value(lit("port")).toInt(), tr("enterprise controller"), 1);
        checkedPorts << PortInfo(ui->mediaProxyPortSpinBox->value(), m_appServerSettings.value(lit("proxyPort"), DEFAUT_PROXY_PORT).toInt(), tr("media proxy"), 1);
    }

    if (m_mediaServerHandle)
    {
        checkedPorts << PortInfo(ui->rtspPortLineEdit->text().toInt(), m_mediaServerSettings.value(lit("rtspPort")).toInt(), tr("media server RTSP"), 0);
#ifndef USE_SINGLE_STREAMING_PORT
        checkedPorts << PortInfo(ui->apiPortLineEdit->text().toInt(), m_mediaServerSettings.value(lit("apiPort")).toInt(), tr("media server API"), 0);
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
                QMessageBox::warning(this, tr("Systray"), tr("%1 port is same as %2 port").arg(checkedPorts[i].descriptor).arg(checkedPorts[j].descriptor));
                return false;
            }
        }
        bool isNewPort = true;
        for (int j = 0; j < checkedPorts.size(); ++j) {
            if (checkedPorts[j].oldPort == checkedPorts[i].newPort)
                isNewPort = false;
        }
        if (isNewPort && !isPortFree(checkedPorts[i].newPort)) {
            QMessageBox::warning(this, tr("Systray"), tr("%1 port already used by another process").arg(checkedPorts[i].descriptor));
            return false;
        }
    }
    QString staticPublicIP = ui->staticPublicIPEdit->text().trimmed();
    if (!staticPublicIP.isEmpty() && QHostAddress(staticPublicIP).isNull())
    {
        QMessageBox::warning(this, tr("Systray"), tr("Invalid IP address specified for public IP address"));
        return false;
    }

    return true;
}

void QnSystrayWindow::saveData()
{
    m_mediaServerSettings.setValue(lit("appserverLogin"), ui->appServerLogin->text());
    m_mediaServerSettings.setValue(lit("appserverPassword"),ui->appServerPassword->text());
    m_mediaServerSettings.setValue(lit("rtspPort"), ui->rtspPortLineEdit->text());
#ifndef USE_SINGLE_STREAMING_PORT
    m_mediaServerSettings.setValue(lit("apiPort"), ui->apiPortLineEdit->text());
#endif

    m_mediaServerSettings.setValue(lit("rtspTransport"), ui->rtspTransportComboBox->currentText());

    m_appServerSettings.setValue(lit("port"), QString::number(ui->ecsPortSpinBox->value()));
    m_appServerSettings.setValue(lit("proxyPort"), QString::number(ui->mediaProxyPortSpinBox->value()));

    if (!ui->ecsAllowPublicIpGroupBox->isChecked())
        m_appServerSettings.setValue(lit("publicIpMode"), ECS_PUBLIC_IP_MODE_DISABLED);
    else if (ui->ecsUseAutoPublicIp->isChecked())
        m_appServerSettings.setValue(lit("publicIpMode"), ECS_PUBLIC_IP_MODE_AUTO);
    else
        m_appServerSettings.setValue(lit("publicIpMode"), ECS_PUBLIC_IP_MODE_MANUAL);

    m_appServerSettings.setValue(lit("manualPublicIp"), ui->ecsManuaPublicIPEdit->text());
    QStringList urlList = m_settings.value(lit("appserverUrlHistory")).toString().split(lit(';'));
    urlList.insert(0, getAppServerURL().toString());
    urlList.removeDuplicates();
    QString rez;
    foreach(QString str, urlList)
    {
        str = str.trimmed();
        if (!str.isEmpty()) {
            if (!rez.isEmpty())
                rez += lit(';');
            rez += str;
        }
    }
    m_settings.setValue(lit("appserverUrlHistory"), rez);

    setAppServerURL(QString(lit("https://%1:%2")).arg(ui->appIPEdit->text()).arg(ui->appPortSpinBox->value()) );


    m_mediaServerSettings.setValue(lit("staticPublicIP"), ui->staticPublicIPEdit->text());
    if (!ui->groupBoxPublicIP->isChecked())
        m_mediaServerSettings.setValue(lit("publicIPEnabled"), 0);
    else if (ui->radioButtonPublicIPAuto->isChecked())
        m_mediaServerSettings.setValue(lit("publicIPEnabled"), 1);
    else
        m_mediaServerSettings.setValue(lit("publicIPEnabled"), 2);
}

void QnSystrayWindow::onTestButtonClicked()
{
    QUrl url( QString(lit("https://%1:%2")).arg(ui->appIPEdit->text()).arg(ui->appPortSpinBox->value()) );

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
