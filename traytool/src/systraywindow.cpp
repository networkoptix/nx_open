#include <QtGui>

 #include "systraywindow.h"
#include "ui_settings.h"

QnSystrayWindow::QnSystrayWindow():
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

    m_iconOK = QIcon(":/images/heart.png");
    m_iconBad = QIcon(":/images/bad.png");

    m_mediaServerHandle = 0;
    m_appServerHandle = 0;

    m_mediaServerStartAction = 0;
    m_mediaServerStopAction = 0;
    m_appServerStartAction = 0;
    m_appServerStopAction = 0;
    m_scManager = 0;
    m_firstTimeToolTipError = true;



    //createIconGroupBox();
    //createMessageGroupBox();

    findServiceInfo();

    //iconLabel->setMinimumWidth(durationLabel->sizeHint().width());

    createActions();
    createTrayIcon();


    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton * )), this, SLOT(buttonClicked(QAbstractButton * )));

    //connect(showMessageButton, SIGNAL(clicked()), this, SLOT(showMessage()));
    //connect(showIconCheckBox, SIGNAL(toggled(bool)),trayIcon, SLOT(setVisible(bool)));
    //connect(iconComboBox, SIGNAL(currentIndexChanged(int)),this, SLOT(setIcon(int)));
    connect(trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
    this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    //mainLayout->addWidget(iconGroupBox);
    //mainLayout->addWidget(messageGroupBox);
    setLayout(mainLayout);

    //iconComboBox->setCurrentIndex(1);
    trayIcon->show();

    setWindowTitle(tr("VMS settings"));
    resize(400, 300);

    connect (&m_findServices, SIGNAL(timeout()), this, SLOT(findServiceInfo()));
    connect (&m_updateServiceStatus, SIGNAL(timeout()), this, SLOT(updateServiceInfo()));
    m_findServices.start(10000);
    m_updateServiceStatus.start(500);
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

void QnSystrayWindow::findServiceInfo()
{
    if (m_scManager == NULL)
        m_scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
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
            trayIcon->setToolTip("Insufficient permissions to start/stop services");
            trayIcon->setIcon(m_iconBad);
            m_firstTimeToolTipError = false;
        }
        return;
    }

    if (m_mediaServerHandle == 0)
        m_mediaServerHandle = OpenService(m_scManager, L"VmsMediaServer", SERVICE_ALL_ACCESS);
    if (m_appServerHandle == 0)
        m_appServerHandle  = OpenService(m_scManager, L"VmsAppServer",   SERVICE_ALL_ACCESS);
}

 void QnSystrayWindow::setVisible(bool visible)
 {
     //minimizeAction->setEnabled(visible);
     //maximizeAction->setEnabled(!isMaximized());
     restoreAction->setEnabled(isMaximized() || !visible);
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
     switch (reason) {
     case QSystemTrayIcon::Trigger:
     case QSystemTrayIcon::DoubleClick:
         //iconComboBox->setCurrentIndex((iconComboBox->currentIndex() + 1)
         //                              % iconComboBox->count());
         break;
     case QSystemTrayIcon::MiddleClick:
         showMessage();
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
     QMessageBox::information(0, qApp->applicationName(), m_detailedErrorText);
       
 }

/*
 void QnSystrayWindow::createIconGroupBox()
 {
     iconGroupBox = new QGroupBox(tr("Tray Icon"));

     iconLabel = new QLabel("Icon:");

     iconComboBox = new QComboBox;
     iconComboBox->addItem(QIcon(":/images/bad.png"), tr("Bad"));
     iconComboBox->addItem(QIcon(":/images/heart.png"), tr("Heart"));
     iconComboBox->addItem(QIcon(":/images/trash.png"), tr("Trash"));

     showIconCheckBox = new QCheckBox(tr("Show icon"));
     showIconCheckBox->setChecked(true);

     QHBoxLayout *iconLayout = new QHBoxLayout;
     iconLayout->addWidget(iconLabel);
     iconLayout->addWidget(iconComboBox);
     iconLayout->addStretch();
     iconLayout->addWidget(showIconCheckBox);
     iconGroupBox->setLayout(iconLayout);
 }

 void QnSystrayWindow::createMessageGroupBox()
 {
     messageGroupBox = new QGroupBox(tr("Balloon Message"));

     typeLabel = new QLabel(tr("Type:"));

     typeComboBox = new QComboBox;
     typeComboBox->addItem(tr("None"), QSystemTrayIcon::NoIcon);
     typeComboBox->addItem(style()->standardIcon(
             QStyle::SP_MessageBoxInformation), tr("Information"),
             QSystemTrayIcon::Information);
     typeComboBox->addItem(style()->standardIcon(
             QStyle::SP_MessageBoxWarning), tr("Warning"),
             QSystemTrayIcon::Warning);
     typeComboBox->addItem(style()->standardIcon(
             QStyle::SP_MessageBoxCritical), tr("Critical"),
             QSystemTrayIcon::Critical);
     typeComboBox->setCurrentIndex(1);

     durationLabel = new QLabel(tr("Duration:"));

     durationSpinBox = new QSpinBox;
     durationSpinBox->setRange(5, 60);
     durationSpinBox->setSuffix(" s");
     durationSpinBox->setValue(15);

     durationWarningLabel = new QLabel(tr("(some systems might ignore this "
                                          "hint)"));
     durationWarningLabel->setIndent(10);

     titleLabel = new QLabel(tr("Title:"));

     titleEdit = new QLineEdit(tr("Cannot connect to network"));

     bodyLabel = new QLabel(tr("Body:"));

     bodyEdit = new QTextEdit;
     bodyEdit->setPlainText(tr("Don't believe me. Honestly, I don't have a "
                               "clue.\nClick this balloon for details."));

     showMessageButton = new QPushButton(tr("Show Message"));
     showMessageButton->setDefault(true);

     QGridLayout *messageLayout = new QGridLayout;
     messageLayout->addWidget(typeLabel, 0, 0);
     messageLayout->addWidget(typeComboBox, 0, 1, 1, 2);
     messageLayout->addWidget(durationLabel, 1, 0);
     messageLayout->addWidget(durationSpinBox, 1, 1);
     messageLayout->addWidget(durationWarningLabel, 1, 2, 1, 3);
     messageLayout->addWidget(titleLabel, 2, 0);
     messageLayout->addWidget(titleEdit, 2, 1, 1, 4);
     messageLayout->addWidget(bodyLabel, 3, 0);
     messageLayout->addWidget(bodyEdit, 3, 1, 2, 4);
     messageLayout->addWidget(showMessageButton, 5, 4);
     messageLayout->setColumnStretch(3, 1);
     messageLayout->setRowStretch(4, 1);
     messageGroupBox->setLayout(messageLayout);
 }
 */

void QnSystrayWindow::updateServiceInfo()
{
    updateServiceInfoInternal(m_mediaServerHandle, "VMS Media Server",       m_mediaServerStartAction, m_mediaServerStopAction);
    updateServiceInfoInternal(m_appServerHandle,   "VMS Application Server", m_appServerStartAction,   m_appServerStopAction);
}

void QnSystrayWindow::updateServiceInfoInternal(SC_HANDLE service, const QString& serviceName, QAction* startAction, QAction* stopAction)
{
    SERVICE_STATUS serviceStatus;

    if (!service)
    {
        stopAction->setVisible(false);
        startAction->setVisible(false);
        return;
    }

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
}

void QnSystrayWindow::mediaServerStartAction()
{
    if (m_mediaServerHandle)
        StartService(m_mediaServerHandle, NULL, 0);
    updateServiceInfo();
}

void QnSystrayWindow::mediaServerStopAction()
{
    SERVICE_STATUS serviceStatus;
    if (m_mediaServerHandle)
        ControlService(m_mediaServerHandle, SERVICE_CONTROL_STOP, &serviceStatus);
    updateServiceInfo();
}

void QnSystrayWindow::appServerStartAction()
{
    if (m_appServerHandle)
        StartService(m_appServerHandle, NULL, 0);
}

void QnSystrayWindow::appServerStopAction()
{
    SERVICE_STATUS serviceStatus;
    if (m_appServerHandle)
        ControlService(m_appServerHandle, SERVICE_CONTROL_STOP, &serviceStatus);
    updateServiceInfo();
}

 void QnSystrayWindow::createActions()
 {
     //minimizeAction = new QAction(tr("Mi&nimize"), this);
     //connect(minimizeAction, SIGNAL(triggered()), this, SLOT(hide()));

     //maximizeAction = new QAction(tr("Ma&ximize"), this);
     //connect(maximizeAction, SIGNAL(triggered()), this, SLOT(showMaximized()));

     restoreAction = new QAction(tr("&Settings"), this);
     connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));

     quitAction = new QAction(tr("&Quit"), this);
     connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

     m_mediaServerStartAction = new QAction(tr("Start VMS MediaServer"), this);
     connect(m_mediaServerStartAction, SIGNAL(triggered()), this, SLOT(mediaServerStartAction()));

     m_mediaServerStopAction = new QAction(tr("Stop VMS MediaServer"), this);
     connect(m_mediaServerStopAction, SIGNAL(triggered()), this, SLOT(mediaServerStopAction()));

     m_appServerStartAction = new QAction(tr("Start VMS AppServer"), this);
     connect(m_appServerStartAction, SIGNAL(triggered()), this, SLOT(appServerStartAction()));

     m_appServerStopAction = new QAction(tr("Stop VMS AppServer"), this);
     connect(m_appServerStopAction, SIGNAL(triggered()), this, SLOT(appServerStopAction()));

     updateServiceInfo();
 }

 void QnSystrayWindow::createTrayIcon()
 {
     trayIconMenu = new QMenu(this);

     trayIconMenu->addAction(m_mediaServerStartAction);
     trayIconMenu->addAction(m_mediaServerStopAction);
     trayIconMenu->addAction(m_appServerStartAction);
     trayIconMenu->addAction(m_appServerStopAction);
     trayIconMenu->addSeparator();

     //trayIconMenu->addAction(minimizeAction);
     trayIconMenu->addAction(restoreAction);
     trayIconMenu->addSeparator();

     trayIconMenu->addAction(quitAction);

     trayIcon = new QSystemTrayIcon(this);
     trayIcon->setContextMenu(trayIconMenu);
     trayIcon->setIcon(m_iconOK);
 }

 void QnSystrayWindow::buttonClicked(QAbstractButton * button)
 {
    int gg = 4;
 }
