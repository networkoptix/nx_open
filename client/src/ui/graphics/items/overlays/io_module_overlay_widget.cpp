#include "io_module_overlay_widget.h"

#include <QtWidgets/QGraphicsRectItem>

#include <core/resource/camera_resource.h>
#include <api/app_server_connection.h>
#include <camera/iomodule/iomodule_monitor.h>
#include <business/business_event_parameters.h>
#include <business/business_action_parameters.h>
#include <business/actions/camera_output_business_action.h>
#include <utils/common/connective.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>
#include <ui/style/skin.h>

namespace {
    const int reconnectDelay = 1000 * 5;
    const int commandExecuteTimeout = 1000 * 6;
}

class QnIoModuleOverlayWidgetPrivate : public Connective<QObject> {
    typedef Connective<QObject> base_type;

public:
    QnIoModuleOverlayWidget *widget;
    QGraphicsLinearLayout *inputsLayout;
    QGraphicsLinearLayout *outputsLayout;
    QnVirtualCameraResourcePtr camera;
    QnIOModuleMonitorPtr monitor;

    struct ModelData {
        ModelData(): widget(0) { buttonPressTime.invalidate(); }

        QnIOPortData ioConfigData;      // port configurating parameters: name e.t.c
        QnIOStateData ioState;          // current port state on or off
        QWidget *widget;                // associated widget
        QElapsedTimer buttonPressTime;  // button press timeout
    };

    QMap<QString, ModelData> model;
    QTimer *timer;

    QnIoModuleOverlayWidgetPrivate(QnIoModuleOverlayWidget *parent);

    void setCamera(const QnVirtualCameraResourcePtr &camera);
    void addIOItem(const QString &name, const QString &description, bool button);
    void updateControls();

    void openConnection();

    void at_cameraStatusChanged(const QnResourcePtr &resource);
    void at_cameraPropertyChanged(const QnResourcePtr &resource, const QString &key);
    void at_connectionOpened();
    void at_connectionClosed();
    void at_ioStateChanged(const QnIOStateData &value);
    void at_buttonStateChanged(bool toggled);
    void at_buttonClicked();
    void at_timer();
};

QnIoModuleOverlayWidgetPrivate::QnIoModuleOverlayWidgetPrivate(QnIoModuleOverlayWidget *parent)
    : base_type(parent)
    , widget(parent)
{
    widget->setAutoFillBackground(true);

    inputsLayout = new QGraphicsLinearLayout();
    inputsLayout->setOrientation(Qt::Vertical);

    outputsLayout = new QGraphicsLinearLayout();
    outputsLayout->setOrientation(Qt::Vertical);

    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout();
    mainLayout->setOrientation(Qt::Horizontal);
    mainLayout->addItem(inputsLayout);
    mainLayout->setStretchFactor(inputsLayout, 1);
    mainLayout->addItem(outputsLayout);
    mainLayout->setStretchFactor(outputsLayout, 1);

    widget->setLayout(mainLayout);
}

void QnIoModuleOverlayWidgetPrivate::setCamera(const QnVirtualCameraResourcePtr &camera) {
    this->camera = camera;

    if (!camera) {
        monitor.reset();
        widget->setEnabled(false);
        return;
    }

    monitor.reset(new QnIOModuleMonitor(camera));
    connect(monitor,    &QnIOModuleMonitor::connectionOpened,   this,   &QnIoModuleOverlayWidgetPrivate::at_connectionOpened);
    connect(monitor,    &QnIOModuleMonitor::connectionClosed,   this,   &QnIoModuleOverlayWidgetPrivate::at_connectionClosed);
    connect(monitor,    &QnIOModuleMonitor::ioStateChanged,     this,   &QnIoModuleOverlayWidgetPrivate::at_ioStateChanged);

    connect(camera,     &QnResource::propertyChanged,   this,       &QnIoModuleOverlayWidgetPrivate::at_cameraPropertyChanged);
    connect(camera,     &QnResource::statusChanged,     this,       &QnIoModuleOverlayWidgetPrivate::at_cameraStatusChanged);

    connect(timer,      &QTimer::timeout,       this,       &QnIoModuleOverlayWidgetPrivate::at_timer);
    timer->start(1000);

    updateControls();
}

void QnIoModuleOverlayWidgetPrivate::addIOItem(const QString &name, const QString &description, bool button) {

}

void QnIoModuleOverlayWidgetPrivate::updateControls() {
    while (inputsLayout->count() > 0)
        inputsLayout->removeAt(0);
    while (outputsLayout->count() > 0)
        outputsLayout->removeAt(0);

    for (const auto &value: camera->getIOPorts())
        model[value.id].ioConfigData = value;

    for (auto it = model.begin(); it != model.end(); ++it) {
        const QnIOPortData &portConfigData = it->ioConfigData;
        if (portConfigData.portType != Qn::PT_Input && portConfigData.portType != Qn::PT_Output)
            continue;

        addIOItem(portConfigData.id, portConfigData.inputName, portConfigData.portType == Qn::PT_Output);
    }
}

void QnIoModuleOverlayWidgetPrivate::openConnection() {
    if (!monitor->open())
        at_connectionClosed();
}

void QnIoModuleOverlayWidgetPrivate::at_cameraStatusChanged(const QnResourcePtr &resource) {
    if (resource->getStatus() < Qn::Online)
        at_connectionClosed();
}

void QnIoModuleOverlayWidgetPrivate::at_cameraPropertyChanged(const QnResourcePtr &resource, const QString &key) {
    Q_UNUSED(resource)
    if (key == Qn::IO_SETTINGS_PARAM_NAME)
        updateControls();
}

void QnIoModuleOverlayWidgetPrivate::at_connectionOpened() {
    widget->setEnabled(true);
}

void QnIoModuleOverlayWidgetPrivate::at_connectionClosed() {
    widget->setEnabled(false);
    for (auto it = model.begin(); it != model.end(); ++it)
        it->buttonPressTime.invalidate();

    executeDelayed([this](){ openConnection(); }, reconnectDelay);
}

void QnIoModuleOverlayWidgetPrivate::at_ioStateChanged(const QnIOStateData &value) {
    auto it = model.find(value.id);
    if (it == model.end())
        return;

    it->buttonPressTime.invalidate();
    if (QLabel *label = qobject_cast<QLabel*>(it->widget)) {
        QPixmap pixmap;
        if (value.isActive)
            pixmap = qnSkin->pixmap("item/io_indicator_on.png");
        else
            pixmap = qnSkin->pixmap("item/io_indicator_off.png");
        label->setPixmap(pixmap);
    } else if (QPushButton *button = qobject_cast<QPushButton*>(it->widget)) {
        button->setChecked(value.isActive);
    }
}

void QnIoModuleOverlayWidgetPrivate::at_buttonStateChanged(bool toggled) {
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    QIcon icon;
    if (toggled)
        icon = qnSkin->icon("item/io_indicator_on.png");
    else
        icon = qnSkin->icon("item/io_indicator_off.png");
    button->setIcon(icon);
}

void QnIoModuleOverlayWidgetPrivate::at_buttonClicked() {
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button)
        return;

    for (auto it = model.begin(); it != model.end(); ++it)  {
        if (it->widget == button)  {
            auto ioConfigData = it->ioConfigData;

            QnBusinessEventParameters eventParams;
            eventParams.setEventTimestamp(qnSyncTime->currentMSecsSinceEpoch());

            QnBusinessActionParameters params;
            params.relayOutputId = ioConfigData.id;
            params.relayAutoResetTimeout = ioConfigData.autoResetTimeoutMs;
            bool isInstant = true; //setting.autoResetTimeoutMs != 0;
            QnCameraOutputBusinessActionPtr action(new QnCameraOutputBusinessAction(isInstant, eventParams));

            action->setParams(params);
            action->setResources(QVector<QnUuid>() << camera->getId());
            action->setToggleState(button->isChecked() ? QnBusiness::ActiveState : QnBusiness::InactiveState);

            ec2::AbstractECConnectionPtr connection = QnAppServerConnectionFactory::getConnection2();
            // we are not interested in client->server transport error code because of real port checking by timer
            if (connection)
                connection->getBusinessEventManager()->sendBusinessAction(action, camera->getParentId(), this, []{});

            it->buttonPressTime.restart();

            break;
        }
    }
}

void QnIoModuleOverlayWidgetPrivate::at_timer() {
    for (auto it = model.begin(); it != model.end(); ++it) {
        ModelData &data = it.value();
        QPushButton *button = qobject_cast<QPushButton*>(data.widget);
        if (button && button->isChecked() != data.ioState.isActive && data.buttonPressTime.isValid() && data.buttonPressTime.hasExpired(commandExecuteTimeout)) {
            bool wrongState = button->isCheckable();
            button->setChecked(data.ioState.isActive);
            data.buttonPressTime.invalidate();
            QString portName = data.ioConfigData.getName();
            QString message = wrongState ? tr("Failed to turn on IO port '%2'")
                                         : tr("Failed to turn off IO port '%2'");
            QMessageBox::warning(0, tr("IO port error"), message.arg(portName));
        }
    }
}


QnIoModuleOverlayWidget::QnIoModuleOverlayWidget(QGraphicsWidget *parent)
    : base_type(parent)
    , d(new QnIoModuleOverlayWidgetPrivate(this))
{

}

QnIoModuleOverlayWidget::~QnIoModuleOverlayWidget() {}

void QnIoModuleOverlayWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    d->setCamera(camera);
}


