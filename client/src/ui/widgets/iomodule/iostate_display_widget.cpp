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

    auto ioSettings = m_camera->getIOPorts();

    //QGridLayout* mainLayout = new QGridLayout(this);
    //setLayout(mainLayout);

    QFont labelFont = font();
    labelFont.setBold(true);

    int row = 0;
    for (const QnIOPortData& value: ioSettings)
    {
        if (value.portType != Qn::PT_Input)
            continue;


        QLabel* idLabel = new QLabel(this);
        idLabel->setText(value.id);
        idLabel->setFont(labelFont);
        idLabel->setStyleSheet(lit("QLabel { color : #80ffffff; }"));
        ui->gridLayout->addWidget(idLabel, row, 0);

        QLabel* inputStateLabel = new QLabel(this);
        QPixmap pixmap = qnSkin->pixmap("item/io_indicator_on.png");
        inputStateLabel->setPixmap(pixmap);
        inputStateLabel->setFixedSize(pixmap.size());
        ui->gridLayout->addWidget(inputStateLabel, row, 1);

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
    for (const QnIOPortData& value: ioSettings)
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

        row++;
    }

    /*
    for (const QnIOPortData& value: ioSettings) 
    {
        if (value.portType == Qn::PT_Disabled)
            continue;

        //QHBoxLayout* layout = new QHBoxLayout(this);
        //layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
        //layout->setSpacing(6.0);

        QFont idLabelFont = font();
        idLabelFont.setBold(true);

        QLabel* idLabel = new QLabel(this);
        idLabel->setText(value.id);
        idLabel->setFont(idLabelFont);
        idLabel->setStyleSheet(lit("QLabel { color : #80ffffff; }"));
        layout->addWidget(idLabel);

        if (value.portType == Qn::PT_Input) 
        {
            QFont labelFont = font();
            labelFont.setBold(true);

            QLabel* inputStateLabel = new QLabel(this);
            inputStateLabel->setFont(labelFont);
            QPixmap pixmap = qnSkin->pixmap("item/io_indicator_on.png");
            inputStateLabel->setPixmap(pixmap);
            inputStateLabel->setFixedSize(pixmap.size());
            layout->addWidget(inputStateLabel);

            QLabel* inputTextLabel = new QLabel(this);
            inputTextLabel->setText(value.inputName);
            inputTextLabel->setFont(labelFont);
            layout->addWidget(inputTextLabel);
            
            //QSpacerItem* spacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
            //layout->addSpacerItem(spacer);

            ui->inputLayout->addLayout(layout);
        }
        else {
            QPushButton* button = new QPushButton(this);
            button->setText(value.outputName);
            layout->addWidget(button);

            //QSpacerItem* spacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
            //layout->addSpacerItem(spacer);

            ui->outputLayout->addLayout(layout);
        }

        //QSpacerItem* spacer1 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);
        //ui->verticalLayout->addSpacerItem(spacer1);
    }
    */

    openConnection();
}

void QnIOStateDisplayWidget::at_ioStateChanged(const QnIOStateData& value)
{

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
