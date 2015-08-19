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

        void setOn(bool on) {
            if (on == m_isOn)
                return;

            m_isOn = on;
            setPixmap(m_isOn ? m_onPixmap : m_offPixmap);
        }
    };

    void clearLayout(QGraphicsLayout *layout) {
        while (layout->count() > 0) {
            QGraphicsLayoutItem *item = layout->itemAt(0);
            layout->removeAt(0);
            delete item;
        }
    }
}

class QnIoModuleOverlayWidgetPrivate : public Connective<QObject> {
    typedef Connective<QObject> base_type;

    Q_DISABLE_COPY(QnIoModuleOverlayWidgetPrivate)
    Q_DECLARE_PUBLIC(QnIoModuleOverlayWidget)

public:
    QnIoModuleOverlayWidget * const q_ptr;
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

    QnIoModuleOverlayWidgetPrivate(QnIoModuleOverlayWidget *widget);

    void setCamera(const QnVirtualCameraResourcePtr &camera);
    void addIoItem(ModelData *data);
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

QnIoModuleOverlayWidgetPrivate::QnIoModuleOverlayWidgetPrivate(QnIoModuleOverlayWidget *widget)
    : base_type(widget)
    , q_ptr(widget)
    , inputsLayout(nullptr)
    , outputsLayout(nullptr)
    , connectionOpened(false)
    , indicatorOnPixmap(qnSkin->pixmap("item/io_indicator_on.png"))
    , indicatorOffPixmap(qnSkin->pixmap("item/io_indicator_off.png"))
    , timer(new QTimer(this))
{
    widget->setAutoFillBackground(true);

    inputsLayout = new QGraphicsGridLayout();
    inputsLayout->setColumnAlignment(0, Qt::AlignRight);
    inputsLayout->setColumnAlignment(1, Qt::AlignCenter);
    inputsLayout->setColumnAlignment(2, Qt::AlignLeft);
    inputsLayout->setColumnStretchFactor(2, 1);

    outputsLayout = new QGraphicsGridLayout();
    outputsLayout->setColumnAlignment(0, Qt::AlignRight);
    outputsLayout->setColumnAlignment(1, Qt::AlignCenter);
    outputsLayout->setColumnAlignment(2, Qt::AlignLeft);
    outputsLayout->setColumnStretchFactor(2, 1);

    leftLayout = new QGraphicsLinearLayout();
    leftLayout->setOrientation(Qt::Vertical);
    leftLayout->addItem(inputsLayout);
    leftLayout->addStretch();

    rightLayout = new QGraphicsLinearLayout();
    rightLayout->setOrientation(Qt::Vertical);
    rightLayout->addItem(outputsLayout);
    rightLayout->addStretch();

    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout();
    mainLayout->setOrientation(Qt::Horizontal);
    mainLayout->addItem(leftLayout);
    mainLayout->setStretchFactor(leftLayout, 1);
    mainLayout->addItem(rightLayout);
    mainLayout->setStretchFactor(rightLayout, 1);
    mainLayout->setContentsMargins(24, 48, 24, 24);
    mainLayout->setSpacing(8);
    mainLayout->addStretch();

    widget->setLayout(mainLayout);

    connect(timer,  &QTimer::timeout,   this,   &QnIoModuleOverlayWidgetPrivate::at_timerTimeout);
    timer->setInterval(stateCheckInterval);
}

void QnIoModuleOverlayWidgetPrivate::setCamera(const QnVirtualCameraResourcePtr &camera) {
    Q_Q(QnIoModuleOverlayWidget);

    timer->stop();

    if (this->camera)
        disconnect(this->camera, nullptr, this, nullptr);

    this->camera = camera;

    if (!camera) {
        monitor.reset();
        q->setEnabled(false);
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

void QnIoModuleOverlayWidgetPrivate::addIoItem(QnIoModuleOverlayWidgetPrivate::ModelData *data) {
    if (!inputsLayout || !outputsLayout)
        return;

    if (data->ioConfigData.portType != Qn::PT_Input && data->ioConfigData.portType != Qn::PT_Output)
        return;

    Q_Q(QnIoModuleOverlayWidget);

    GraphicsLabel *portIdLabel = new GraphicsLabel(data->ioConfigData.id, q);
    portIdLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);
    portIdLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    QPalette palette = portIdLabel->palette();
    palette.setColor(QPalette::WindowText, colors.idLabel);
    portIdLabel->setPalette(palette);

    data->indicator = new IndicatorWidget(indicatorOnPixmap, indicatorOffPixmap, q);
    data->indicator->setOn(data->ioState.isActive);

    if (data->ioConfigData.portType == Qn::PT_Output) {
        int row = outputsLayout->rowCount();

        QPushButton *button = new QPushButton(data->ioConfigData.outputName);
        button->setProperty(ioPortPropertyName, data->ioConfigData.id);
        button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        button->setAutoDefault(false);
        connect(button, &QPushButton::clicked, this, &QnIoModuleOverlayWidgetPrivate::at_buttonClicked);
        QGraphicsProxyWidget *buttonProxy = new QGraphicsProxyWidget(q);
        buttonProxy->setWidget(button);

        outputsLayout->addItem(portIdLabel, row, 0);
        outputsLayout->addItem(data->indicator, row, 1);
        outputsLayout->addItem(buttonProxy, row, 2);

        outputsLayout->setRowAlignment(row, Qt::AlignCenter);
    } else {
        int row = inputsLayout->rowCount();

        GraphicsLabel *descriptionLabel = new GraphicsLabel(data->ioConfigData.inputName, q);
        descriptionLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        descriptionLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);

        inputsLayout->addItem(portIdLabel, row, 0);
        inputsLayout->addItem(data->indicator, row, 1);
        inputsLayout->addItem(descriptionLabel, row, 2);

        inputsLayout->setRowAlignment(row, Qt::AlignCenter);
    }
}

void QnIoModuleOverlayWidgetPrivate::updateControls() {
    clearLayout(inputsLayout);
    clearLayout(outputsLayout);

    for (const auto &value: camera->getIOPorts())
        model[value.id].ioConfigData = value;

    for (auto it = model.begin(); it != model.end(); ++it)
        addIoItem(&it.value());

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
    Q_Q(QnIoModuleOverlayWidget);
    connectionOpened = true;
    q->setEnabled(true);
}

void QnIoModuleOverlayWidgetPrivate::at_connectionClosed() {
    Q_Q(QnIoModuleOverlayWidget);

    q->setEnabled(false);
    connectionOpened = false;

    for (auto it = model.begin(); it != model.end(); ++it)
        it->buttonPressTime.invalidate();

    executeDelayedParented([this](){ openConnection(); }, reconnectDelay, this);
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
    eventParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();

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
            QString message = wrongState ? tr("Failed to turn on IO port '%1'")
                                         : tr("Failed to turn off IO port '%1'");
            QMessageBox::warning(0, tr("IO port error"), message.arg(portName));
        }
    }
}


QnIoModuleOverlayWidget::QnIoModuleOverlayWidget(QGraphicsWidget *parent)
    : base_type(parent)
    , d_ptr(new QnIoModuleOverlayWidgetPrivate(this))
{
}

QnIoModuleOverlayWidget::~QnIoModuleOverlayWidget() {}

void QnIoModuleOverlayWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    Q_D(QnIoModuleOverlayWidget);
    d->setCamera(camera);
}

const QnIoModuleColors &QnIoModuleOverlayWidget::colors() const {
    Q_D(const QnIoModuleOverlayWidget);
    return d->colors;
}

void QnIoModuleOverlayWidget::setColors(const QnIoModuleColors &colors) {
    Q_D(QnIoModuleOverlayWidget);
    d->colors = colors;
    d->updateControls();
}
