#include "iostate_display_widget.h"
#include "camera/iomodule/iomodule_monitor.h"
#include "ui_iostate_display_widget.h"
#include "ui/style/skin.h"

namespace
{
    const int RECONNECT_DELAY = 1000 * 5;
}

QnIOStateDisplayWidget::QnIOStateDisplayWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::IOStateDisplayWidget())
{
    ui->setupUi(this);

}

void QnIOStateDisplayWidget::setCamera(const QnServerCameraPtr& camera)
{

    m_camera = camera;
    m_monitor = camera->createIOModuleMonitor();
    connect(m_monitor.data(), &QnIOModuleMonitor::connectionOpened, this, &QnIOStateDisplayWidget::at_connectionOpened );
    connect(m_monitor.data(), &QnIOModuleMonitor::connectionClosed, this, &QnIOStateDisplayWidget::at_connectionClosed );
    connect(m_monitor.data(), &QnIOModuleMonitor::ioStateChanged, this, &QnIOStateDisplayWidget::at_ioStateChanged );
    connect(camera.data(), &QnResource::propertyChanged, this, &QnIOStateDisplayWidget::at_cameraPropertyChanged );

    updateControls();
}

void QnIOStateDisplayWidget::at_cameraPropertyChanged(const QnResourcePtr & /*res*/, const QString & key)
{
    if (key == Qn::IO_SETTINGS_PARAM_NAME)
        updateControls();
}

void QnIOStateDisplayWidget::updateControls()
{

    m_ioSettings = m_camera->getIOPorts();
    m_widgetsByPort.clear();

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
        button->setText(value.outputName);
        ui->gridLayout->addWidget(button, row, 6);
        m_widgetsByPort.insert(value.id, button);

        row++;
    }

    openConnection();
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
    }
}

void QnIOStateDisplayWidget::at_connectionOpened()
{

}

void QnIOStateDisplayWidget::at_connectionClosed()
{
    QTimer::singleShot(RECONNECT_DELAY, this, SLOT(QnIOStateDisplayWidget::openConnection()));
}


void QnIOStateDisplayWidget::openConnection()
{
    m_monitor->open();
}
