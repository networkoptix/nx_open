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
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/standard/graphics_pixmap.h>

namespace {
    const int reconnectDelay = 1000 * 5;
    const int commandExecuteTimeout = 1000 * 6;
    const int stateCheckInterval = 1000;
    const char *ioPortPropertyName = "ioPort";

    class IndicatorWidget : public GraphicsPixmap {
        QPixmap m_onPixmap;
        QPixmap m_offPixmap;
        bool m_isOn;

    public:
        IndicatorWidget(const QPixmap &onPixmap, const QPixmap &offPixmap, QGraphicsItem *parent = 0)
            : GraphicsPixmap(parent)
            , m_onPixmap(onPixmap)
            , m_offPixmap(offPixmap)
            , m_isOn(false)
        {
            setPixmap(m_offPixmap);
        }

        bool isOn() const {
            return m_isOn;
        }

        void setOn(bool f) {
            if (f == m_isOn)
                return;

            m_isOn = f;
            setPixmap(m_isOn ? m_onPixmap : m_offPixmap);
        }
    };

}

class QnIoModuleOverlayWidgetPrivate : public Connective<QObject> {
    typedef Connective<QObject> base_type;

public:
    QnIoModuleOverlayWidget *widget;
    QGraphicsLinearLayout *leftLayout;
    QGraphicsLinearLayout *rightLayout;
    QGraphicsGridLayout *inputsLayout;
    QGraphicsGridLayout *outputsLayout;
    QnVirtualCameraResourcePtr camera;
    QnIOModuleMonitorPtr monitor;
    bool connectionOpened;
    QPixmap indicatorOnPixmap;
    QPixmap indicatorOffPixmap;
    QnIoModuleColors colors;

    struct ModelData {
        ModelData() : indicator(0) {}

        QnIOPortData ioConfigData;      // port configurating parameters: name e.t.c
        QnIOStateData ioState;          // current port state on or off
        IndicatorWidget *indicator;
        QElapsedTimer buttonPressTime;  // button press timeout
    };

    QMap<QString, ModelData> model;
    QTimer *timer;

    QnIoModuleOverlayWidgetPrivate(QnIoModuleOverlayWidget *parent);

    void setCamera(const QnVirtualCameraResourcePtr &camera);
    void addIOItem(ModelData *data);
    void updateControls();

    void openConnection();

    void at_cameraStatusChanged(const QnResourcePtr &resource);
    void at_cameraPropertyChanged(const QnResourcePtr &resource, const QString &key);
    void at_connectionOpened();
    void at_connectionClosed();
    void at_ioStateChanged(const QnIOStateData &value);
    void at_buttonClicked();
    void at_timerTimeout();
};

QnIoModuleOverlayWidgetPrivate::QnIoModuleOverlayWidgetPrivate(QnIoModuleOverlayWidget *parent)
    : base_type(parent)
    , widget(parent)
    , inputsLayout(nullptr)
    , outputsLayout(nullptr)
    , connectionOpened(false)
    , indicatorOnPixmap(qnSkin->pixmap("item/io_indicator_on.png"))
    , indicatorOffPixmap(qnSkin->pixmap("item/io_indicator_off.png"))
    , timer(new QTimer(this))
{
    widget->setAutoFillBackground(true);

    leftLayout = new QGraphicsLinearLayout();
    leftLayout->setOrientation(Qt::Vertical);
    leftLayout->addStretch();
    rightLayout = new QGraphicsLinearLayout();
    rightLayout->setOrientation(Qt::Vertical);
    rightLayout->addStretch();

    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout();
    mainLayout->setOrientation(Qt::Horizontal);
    mainLayout->addItem(leftLayout);
    mainLayout->setStretchFactor(leftLayout, 1);
    mainLayout->addItem(rightLayout);
    mainLayout->setStretchFactor(rightLayout, 1);
    mainLayout->setContentsMargins(24, 48, 24, 24);
    mainLayout->setSpacing(8);

    widget->setLayout(mainLayout);

    connect(timer,  &QTimer::timeout,   this,   &QnIoModuleOverlayWidgetPrivate::at_timerTimeout);
    timer->setInterval(stateCheckInterval);
}

void QnIoModuleOverlayWidgetPrivate::setCamera(const QnVirtualCameraResourcePtr &camera) {
    timer->stop();

    if (this->camera)
        disconnect(this->camera, nullptr, this, nullptr);

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

    timer->start();

    updateControls();
}

void QnIoModuleOverlayWidgetPrivate::addIOItem(QnIoModuleOverlayWidgetPrivate::ModelData *data) {
    if (!inputsLayout || !outputsLayout)
        return;

    if (data->ioConfigData.portType != Qn::PT_Input && data->ioConfigData.portType != Qn::PT_Output)
        return;

    GraphicsLabel *portIdLabel = new GraphicsLabel(data->ioConfigData.id, widget);
    portIdLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);
    portIdLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    QPalette palette = portIdLabel->palette();
    palette.setColor(QPalette::WindowText, colors.idLabel);
    portIdLabel->setPalette(palette);

    data->indicator = new IndicatorWidget(indicatorOnPixmap, indicatorOffPixmap, widget);
    data->indicator->setOn(data->ioState.isActive);

    if (data->ioConfigData.portType == Qn::PT_Output) {
        int row = outputsLayout->rowCount();

        QPushButton *button = new QPushButton(data->ioConfigData.outputName);
        button->setProperty(ioPortPropertyName, data->ioConfigData.id);
        button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        button->setAutoDefault(false);
        connect(button, &QPushButton::clicked, this, &QnIoModuleOverlayWidgetPrivate::at_buttonClicked);
        QGraphicsProxyWidget *buttonProxy = new QGraphicsProxyWidget(widget);
        buttonProxy->setWidget(button);

        outputsLayout->addItem(portIdLabel, row, 0);
        outputsLayout->addItem(data->indicator, row, 1);
        outputsLayout->addItem(buttonProxy, row, 2);

        outputsLayout->setRowAlignment(row, Qt::AlignCenter);
    } else {
        int row = inputsLayout->rowCount();
        GraphicsLabel *descriptionLabel = new GraphicsLabel(data->ioConfigData.inputName, widget);
        descriptionLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        descriptionLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);

        inputsLayout->addItem(portIdLabel, row, 0);
        inputsLayout->addItem(data->indicator, row, 1);
        inputsLayout->addItem(descriptionLabel, row, 2);

        inputsLayout->setRowAlignment(row, Qt::AlignCenter);
    }
}

void QnIoModuleOverlayWidgetPrivate::updateControls() {
    if (inputsLayout) {
        leftLayout->removeItem(inputsLayout);

        while (inputsLayout->count() > 0) {
            QGraphicsLayoutItem *item = inputsLayout->itemAt(0);
            inputsLayout->removeAt(0);
            delete item;
        }

        delete inputsLayout;
    }

    if (outputsLayout) {
        rightLayout->removeItem(outputsLayout);

        while (outputsLayout->count() > 0) {
            QGraphicsLayoutItem *item = outputsLayout->itemAt(0);
            outputsLayout->removeAt(0);
            delete item;
        }

        delete outputsLayout;
    }

    inputsLayout = new QGraphicsGridLayout();
    inputsLayout->setColumnAlignment(0, Qt::AlignRight);
    inputsLayout->setColumnAlignment(1, Qt::AlignCenter);
    inputsLayout->setColumnAlignment(2, Qt::AlignLeft);
    inputsLayout->setColumnStretchFactor(2, 1);
    leftLayout->insertItem(0, inputsLayout);

    outputsLayout = new QGraphicsGridLayout();
    outputsLayout->setColumnAlignment(0, Qt::AlignRight);
    outputsLayout->setColumnAlignment(1, Qt::AlignCenter);
    outputsLayout->setColumnAlignment(2, Qt::AlignLeft);
    outputsLayout->setColumnStretchFactor(2, 1);
    rightLayout->insertItem(0, outputsLayout);

    for (const auto &value: camera->getIOPorts())
        model[value.id].ioConfigData = value;

    for (auto it = model.begin(); it != model.end(); ++it)
        addIOItem(&it.value());

    if (!connectionOpened)
        openConnection();
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
    connectionOpened = true;
    widget->setEnabled(true);
}

void QnIoModuleOverlayWidgetPrivate::at_connectionClosed() {
    widget->setEnabled(false);
    connectionOpened = false;

    for (auto it = model.begin(); it != model.end(); ++it)
        it->buttonPressTime.invalidate();

    executeDelayed([this](){ openConnection(); }, reconnectDelay);
}

void QnIoModuleOverlayWidgetPrivate::at_ioStateChanged(const QnIOStateData &value) {
    auto it = model.find(value.id);
    if (it == model.end())
        return;

    it->buttonPressTime.invalidate();
    it->indicator->setOn(value.isActive);
    it->ioState = value;
}

void QnIoModuleOverlayWidgetPrivate::at_buttonClicked() {
    QString port = sender()->property(ioPortPropertyName).toString();
    if (port.isEmpty())
        return;

    auto it = model.find(port);
    if (it == model.end())
        return;

    QnBusinessEventParameters eventParams;
    eventParams.setEventTimestamp(qnSyncTime->currentMSecsSinceEpoch());

    QnBusinessActionParameters params;
    params.relayOutputId = it->ioConfigData.id;
    params.relayAutoResetTimeout = it->ioConfigData.autoResetTimeoutMs;
    bool isInstant = true; //setting.autoResetTimeoutMs != 0;
    QnCameraOutputBusinessActionPtr action(new QnCameraOutputBusinessAction(isInstant, eventParams));

    action->setParams(params);
    action->setResources(QVector<QnUuid>() << camera->getId());
    action->setToggleState(it->ioState.isActive ? QnBusiness::InactiveState : QnBusiness::ActiveState);

    ec2::AbstractECConnectionPtr connection = QnAppServerConnectionFactory::getConnection2();
    // we are not interested in client->server transport error code because of real port checking by timer
    if (connection)
        connection->getBusinessEventManager()->sendBusinessAction(action, camera->getParentId(), this, []{});

    it->indicator->setOn(!it->ioState.isActive);
    it->buttonPressTime.restart();
}

void QnIoModuleOverlayWidgetPrivate::at_timerTimeout() {
    for (auto it = model.begin(); it != model.end(); ++it) {
        ModelData &data = it.value();
        if (data.indicator->isOn() != data.ioState.isActive && data.buttonPressTime.isValid() && data.buttonPressTime.hasExpired(commandExecuteTimeout)) {
            bool wrongState = data.indicator->isOn();
            data.indicator->setOn(data.ioState.isActive);
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

const QnIoModuleColors &QnIoModuleOverlayWidget::colors() const {
    return d->colors;
}

void QnIoModuleOverlayWidget::setColors(const QnIoModuleColors &colors) {
    d->colors = colors;
    d->updateControls();
}
