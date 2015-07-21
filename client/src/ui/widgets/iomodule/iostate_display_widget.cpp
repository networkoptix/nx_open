#include "iostate_display_widget.h"
#include "camera/iomodule/iomodule_monitor.h"
#include "ui_iostate_display_widget.h"
#include "ui/style/skin.h"
#include "business/business_event_parameters.h"
#include "business/business_action_parameters.h"
#include "utils/common/synctime.h"
#include "business/actions/camera_output_business_action.h"
#include "api/app_server_connection.h"

namespace
{
    const int RECONNECT_DELAY = 1000 * 5;
    const int COMMAND_EXECUTE_TIMEOUT = 1000 * 6;
}

QnIOStateDisplayWidget::QnIOStateDisplayWidget(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::IOStateDisplayWidget())
{
    ui->setupUi(this);
    setEnabled(false);
}

void QnIOStateDisplayWidget::setCamera(const QnServerCameraPtr& camera)
{
    m_camera = camera;
    if (!camera) {
        m_monitor.reset();
        setEnabled(false);
        setWindowTitle(QString());
        return;
    }

    m_monitor = camera->createIOModuleMonitor();
    connect(m_monitor.data(), &QnIOModuleMonitor::connectionOpened, this, &QnIOStateDisplayWidget::at_connectionOpened );
    connect(m_monitor.data(), &QnIOModuleMonitor::connectionClosed, this, &QnIOStateDisplayWidget::at_connectionClosed );
    connect(m_monitor.data(), &QnIOModuleMonitor::ioStateChanged, this, &QnIOStateDisplayWidget::at_ioStateChanged );

    connect(camera.data(), &QnResource::propertyChanged, this, &QnIOStateDisplayWidget::at_cameraPropertyChanged );
    connect(camera.data(), &QnResource::statusChanged, this, &QnIOStateDisplayWidget::at_cameraStatusChanged );
    connect(camera.data(), &QnResource::nameChanged, this, [this]() {setWindowTitle(m_camera->getName());} );

    connect (&m_timer, &QTimer::timeout, this, &QnIOStateDisplayWidget::at_timer);
    m_timer.start(1000);

    connect(this, &QnIOStateDisplayWidget::close, this, [this]() {deleteLater();} );
    

    updateControls();
}

void QnIOStateDisplayWidget::at_cameraStatusChanged(const QnResourcePtr & res)
{
    if (res->getStatus() < Qn::Online)
        at_connectionClosed();
}

void QnIOStateDisplayWidget::at_cameraPropertyChanged(const QnResourcePtr & /*res*/, const QString & key)
{
    if (key == Qn::IO_SETTINGS_PARAM_NAME)
        updateControls();
}

void QnIOStateDisplayWidget::updateControls()
{
    m_model.clear();
    while ( QObject* w = findChild<QObject*>() )
        delete w;

    ui->setupUi(this);
    setWindowTitle(m_camera->getName());

    for (const auto& value: m_camera->getIOPorts())
        m_model[value.id].ioConfigData = value;


    QFont labelFont = font();
    labelFont.setBold(true);

    int row = 0;
    for (auto itr = m_model.begin(); itr!= m_model.end(); ++itr)
    {
        QnIOPortData portConfigData = itr.value().ioConfigData;
        if (portConfigData.portType != Qn::PT_Input)
            continue;


        QLabel* idLabel = new QLabel(this);
        idLabel->setText(portConfigData.id);
        idLabel->setFont(labelFont);
        idLabel->setStyleSheet(lit("QLabel { color : #80ffffff; }"));
        ui->gridLayout->addWidget(idLabel, row, 0);

        QLabel* inputStateLabel = new QLabel(this);
        QPixmap pixmap = qnSkin->pixmap("item/io_indicator_off.png");
        inputStateLabel->setPixmap(pixmap);
        inputStateLabel->setFixedSize(pixmap.size());
        ui->gridLayout->addWidget(inputStateLabel, row, 1);

        itr.value().widget = inputStateLabel;

        QLabel* inputTextLabel = new QLabel(this);
        inputTextLabel->setText(portConfigData.inputName);
        inputTextLabel->setFont(labelFont);
        ui->gridLayout->addWidget(inputTextLabel, row, 2);

        QLabel* spacerLabel = new QLabel(this);
        spacerLabel->setFixedSize(32, 2);
        ui->gridLayout->addWidget(spacerLabel, row, 3);

        row++;
    }

    row = 0;
    for (auto itr = m_model.begin(); itr!= m_model.end(); ++itr)
    {
        QnIOPortData portConfigData = itr.value().ioConfigData;
        if (portConfigData.portType != Qn::PT_Output)
            continue;

        if (row >= ui->gridLayout->rowCount()) {
            QLabel* spacerLabel = new QLabel(this);
            spacerLabel->setFixedSize(32, 2);
            ui->gridLayout->addWidget(spacerLabel, row, 3);
        }

        QLabel* idLabel = new QLabel(this);
        idLabel->setText(portConfigData.id);
        idLabel->setFont(labelFont);
        idLabel->setStyleSheet(lit("QLabel { color : #80ffffff; }"));
        ui->gridLayout->addWidget(idLabel, row, 4);

        QLabel* spacerLabel = new QLabel(this);
        spacerLabel->setFixedSize(8, 2);
        ui->gridLayout->addWidget(spacerLabel, row, 5);

        QPushButton* button = new QPushButton(this);
        button->setCheckable(true);
        button->setDefault(false);
        button->setText(portConfigData.outputName);
        button->setIcon(qnSkin->icon("item/io_indicator_off.png"));
        connect(button, &QPushButton::clicked, this, &QnIOStateDisplayWidget::at_buttonClicked);
        connect(button, &QPushButton::toggled, this, &QnIOStateDisplayWidget::at_buttonStateChanged);
        ui->gridLayout->addWidget(button, row, 6);

        itr.value().widget = button;

        row++;
    }

    openConnection();
}

void QnIOStateDisplayWidget::at_buttonStateChanged(bool toggled)
{
    QPushButton* button = dynamic_cast<QPushButton*> (sender());
    QIcon icon;
    if (toggled)
        icon = qnSkin->icon("item/io_indicator_on.png");
    else
        icon = qnSkin->icon("item/io_indicator_off.png");
    button->setIcon(icon);
}

void QnIOStateDisplayWidget::at_buttonClicked()
{
    QPushButton* button = dynamic_cast<QPushButton*> (sender());
    if (!button)
        return;
    for (auto itr = m_model.begin(); itr != m_model.end(); ++itr) 
    {
        if (itr.value().widget == button) 
        {
            auto ioConfigData = itr.value().ioConfigData;
            
            QnBusinessEventParameters eventParams;
            eventParams.setEventTimestamp(qnSyncTime->currentMSecsSinceEpoch());

            QnBusinessActionParameters params;
            params.relayOutputId = ioConfigData.id;
            params.relayAutoResetTimeout = ioConfigData.autoResetTimeoutMs;
            bool isInstant = true; //setting.autoResetTimeoutMs != 0;
            QnCameraOutputBusinessActionPtr action(new QnCameraOutputBusinessAction(isInstant, eventParams));

            action->setParams(params);
            action->setResources(QVector<QnUuid>() << m_camera->getId());
            action->setToggleState(button->isChecked() ? QnBusiness::ActiveState : QnBusiness::InactiveState);

            ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
            // we are not interested in client->server transport error code because of real port checking by timer
            if (conn)
                conn->getBusinessEventManager()->sendBusinessAction(action, m_camera->getParentId(), this, []{}); 

            itr.value().btnPressTime.restart();

            break;
        }
    }
}

void QnIOStateDisplayWidget::at_timer()
{
    QStringList errorList;
    for (auto itr = m_model.begin(); itr != m_model.end(); ++itr)
    {
        ModelData& data = itr.value();
        QPushButton* button = dynamic_cast<QPushButton*> (data.widget);
        if (button && button->isChecked() != data.ioState.isActive && data.btnPressTime.isValid() && data.btnPressTime.hasExpired(COMMAND_EXECUTE_TIMEOUT))
        {
            bool wrongState = button->isCheckable();
            button->setChecked(data.ioState.isActive);
            data.btnPressTime.invalidate();
            QString portName = data.ioConfigData.getName();
            QMessageBox::warning(mainWindow(), tr("IO port error"), tr("Failed to %1 IO port '%2'").arg(wrongState ? tr("turn on") : tr("turn off")).arg(portName));
        }
    }
}

void QnIOStateDisplayWidget::at_ioStateChanged(const QnIOStateData& value)
{
    auto itr = m_model.find(value.id);
    if (itr == m_model.end())
        return;

    itr.value().btnPressTime.invalidate();
    if (QLabel* label = dynamic_cast<QLabel*> (itr.value().widget))
    {
        QPixmap pixmap;
        if (value.isActive)
            pixmap = qnSkin->pixmap("item/io_indicator_on.png");
        else
            pixmap = qnSkin->pixmap("item/io_indicator_off.png");
        label->setPixmap(pixmap);
    }
    else if (QPushButton* button = dynamic_cast<QPushButton*> (itr.value().widget))
    {
        button->setChecked(value.isActive);
    }
}

void QnIOStateDisplayWidget::at_connectionOpened()
{
    setEnabled(true);
}

void QnIOStateDisplayWidget::at_connectionClosed()
{
    setEnabled(false);
    for (auto itr = m_model.begin(); itr != m_model.end(); ++itr)
        itr.value().btnPressTime.invalidate();
    QTimer::singleShot(RECONNECT_DELAY, this, SLOT(openConnection()));
}


void QnIOStateDisplayWidget::openConnection()
{
    if (!m_monitor->open())
        at_connectionClosed();
}
