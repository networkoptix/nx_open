#include "iostate_widget.h"
#include "camera/iomodule/iomodule_monitor.h"

namespace
{
    const int RECONNECT_DELAY = 1000 * 5;
}

QnIOStateDisplayWidget::QnIOStateDisplayWidget(QWidget *parent):
    base_type(parent)
{
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
    for (const QnIOPortData& value: ioSettings) 
    {
        if (value.portType == Qn::PT_Disabled)
            continue;

        QHBoxLayout* layout = new QHBoxLayout(this);

        QLabel* idLabel = new QLabel(this);
        idLabel->setText(value.id);
        layout->addWidget(idLabel);

        if (value.portType == Qn::PT_Input) {
            QLabel* inputStateLabel = new QLabel(this);
            //inputStateLabel->setPixmap("io_off");
            QLabel* inputTextLabel = new QLabel(this);
            inputTextLabel->setText(value.inputName);
            layout->addWidget(inputTextLabel);
        }
        else {
            QPushButton* button = new QPushButton(this);
            button->setText(value.outputName);
            layout->addWidget(button);
        }
    }

    openConnection();
}

void QnIOStateDisplayWidget::at_ioStateChanged(const QnIOStateData& value)
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
