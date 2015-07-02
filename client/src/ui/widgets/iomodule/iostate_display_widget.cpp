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
}

QnIOStateDisplayWidget::QnIOStateDisplayWidget(QWidget *parent):
    base_type(parent),
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

    m_ioSettings = m_camera->getIOPorts();
    while ( QObject* w = findChild<QObject*>() )
        delete w;
    ui->setupUi(this);
    m_widgetsByPort.clear();
    setWindowTitle(m_camera->getName());

    //QGridLayout* mainLayout = new QGridLayout(this);
    //setLayout(mainLayout);

    QFont labelFont = font();
    labelFont.setBold(true);

    int row = 0;
    for (const QnIOPortData& value: m_ioSettings)
    {
        if (value.portType != Qn::PT_Input)
            continue;


        QLabel* idLabel = new QLabel(this);
        idLabel->setText(value.id);
        idLabel->setFont(labelFont);
        idLabel->setStyleSheet(lit("QLabel { color : #80ffffff; }"));
        ui->gridLayout->addWidget(idLabel, row, 0);

        QLabel* inputStateLabel = new QLabel(this);
        QPixmap pixmap = qnSkin->pixmap("item/io_indicator_off.png");
        inputStateLabel->setPixmap(pixmap);
        inputStateLabel->setFixedSize(pixmap.size());
        ui->gridLayout->addWidget(inputStateLabel, row, 1);
        m_widgetsByPort.insert(value.id, inputStateLabel);

        QLabel* inputTextLabel = new QLabel(this);
        inputTextLabel->setText(value.inputName);
        inputTextLabel->setFont(labelFont);
        ui->gridLayout->addWidget(inputTextLabel, row, 2);

        QLabel* spacerLabel = new QLabel(this);
        spacerLabel->setFixedSize(32, 2);
        ui->gridLayout->addWidget(spacerLabel, row, 3);

        row++;
    }

    row = 0;
    for (const QnIOPortData& value: m_ioSettings)
    {
        if (value.portType != Qn::PT_Output)
            continue;

        if (row >= ui->gridLayout->rowCount()) {
            QLabel* spacerLabel = new QLabel(this);
            spacerLabel->setFixedSize(32, 2);
            ui->gridLayout->addWidget(spacerLabel, row, 3);
        }

        QLabel* idLabel = new QLabel(this);
        idLabel->setText(value.id);
        idLabel->setFont(labelFont);
        idLabel->setStyleSheet(lit("QLabel { color : #80ffffff; }"));
        ui->gridLayout->addWidget(idLabel, row, 4);

        QLabel* spacerLabel = new QLabel(this);
        spacerLabel->setFixedSize(8, 2);
        ui->gridLayout->addWidget(spacerLabel, row, 5);

        QPushButton* button = new QPushButton(this);
        button->setCheckable(true);
        button->setDefault(false);
        button->setText(value.outputName);
        button->setIcon(qnSkin->icon("item/io_indicator_off.png"));
        connect(button, &QPushButton::clicked, this, &QnIOStateDisplayWidget::at_buttonClicked);
        connect(button, &QPushButton::toggled, this, &QnIOStateDisplayWidget::at_buttonStateChanged);
        ui->gridLayout->addWidget(button, row, 6);
        m_widgetsByPort.insert(value.id, button);

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
    QString portId;
    for (auto itr = m_widgetsByPort.begin(); itr != m_widgetsByPort.end(); ++itr) {
        if (itr.value() == button) {
            portId = itr.key();
            break;
        }
    }
    if (portId.isEmpty())
        return;
    for (const auto& setting: m_ioSettings) {
        if (setting.id == portId) 
        {
            QnBusinessEventParameters eventParams;
            eventParams.setEventTimestamp(qnSyncTime->currentMSecsSinceEpoch());

            QnBusinessActionParameters params;
            params.relayOutputId = setting.id;
            params.relayAutoResetTimeout = setting.autoResetTimeoutMs;
            bool isInstant = true; //setting.autoResetTimeoutMs != 0;
            QnCameraOutputBusinessActionPtr action(new QnCameraOutputBusinessAction(isInstant, eventParams));

            action->setParams(params);
            action->setResources(QVector<QnUuid>() << m_camera->getId());
            action->setToggleState(button->isChecked() ? QnBusiness::ActiveState : QnBusiness::InactiveState);

            ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
            if (conn)
                conn->getBusinessEventManager()->sendBusinessAction(action, m_camera->getParentId(), this, []{});
        }
    }
}

void QnIOStateDisplayWidget::at_ioStateChanged(const QnIOStateData& value)
{
    if (QWidget* widget = m_widgetsByPort.value(value.id))
    {
        if (QLabel* label = dynamic_cast<QLabel*> (widget))
        {
            QPixmap pixmap;
            if (value.isActive)
                pixmap = qnSkin->pixmap("item/io_indicator_on.png");
            else
                pixmap = qnSkin->pixmap("item/io_indicator_off.png");
            label->setPixmap(pixmap);
        }
        else if (QPushButton* button = dynamic_cast<QPushButton*> (widget))
        {
            button->setChecked(value.isActive);
        }
    }
}

void QnIOStateDisplayWidget::at_connectionOpened()
{
    setEnabled(true);
}

void QnIOStateDisplayWidget::at_connectionClosed()
{
    setEnabled(false);
    QTimer::singleShot(RECONNECT_DELAY, this, SLOT(openConnection()));
}


void QnIOStateDisplayWidget::openConnection()
{
    if (!m_monitor->open())
        at_connectionClosed();
}
