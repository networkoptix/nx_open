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

    QGraphicsGridLayout *createGridLayout()
    {
        typedef QPair<int, Qt::Alignment> ColumnInfo;
        static const ColumnInfo kColumns[] = { ColumnInfo(0, Qt::AlignRight)
            , ColumnInfo(1, Qt::AlignCenter), ColumnInfo(2, Qt::AlignLeft) };

        QGraphicsGridLayout * const result = new QGraphicsGridLayout();
        for (auto &column: kColumns)
            result->setColumnAlignment(column.first, column.second);

        return result;
    }

    QString trimName(const QString &name)
    {
        enum { kMaxPortNameLength = 20 };
        if (name.length() <= kMaxPortNameLength)
            return name;

        static const QString kFinalizer = lit("...");
        static const int kFinalizerLength = kFinalizer.length();

        return name.left(kMaxPortNameLength - kFinalizerLength) + kFinalizer;
    }
}

class QnIoModuleOverlayWidgetPrivate : public Connective<QObject> {
    typedef Connective<QObject> base_type;

    Q_DISABLE_COPY(QnIoModuleOverlayWidgetPrivate)
    Q_DECLARE_PUBLIC(QnIoModuleOverlayWidget)
    Q_DECLARE_TR_FUNCTIONS(QnIoModuleOverlayWidgetPrivate)

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

    QGraphicsLayoutItem *createButton(QnIoModuleOverlayWidgetPrivate::ModelData *data);
    QGraphicsLayoutItem *createLabel(QnIoModuleOverlayWidgetPrivate::ModelData *data);
};

QnIoModuleOverlayWidgetPrivate::QnIoModuleOverlayWidgetPrivate(QnIoModuleOverlayWidget *widget)
    : base_type(widget)
    , q_ptr(widget)
    , inputsLayout(createGridLayout())
    , outputsLayout(createGridLayout())
    , connectionOpened(false)
    , indicatorOnPixmap(qnSkin->pixmap("item/io_indicator_on.png"))
    , indicatorOffPixmap(qnSkin->pixmap("item/io_indicator_off.png"))
    , timer(new QTimer(this))
{
    widget->setAutoFillBackground(true);

    enum { kVeryBigStretch = 1000 };
    leftLayout = new QGraphicsLinearLayout();
    leftLayout->setOrientation(Qt::Vertical);
    leftLayout->addItem(inputsLayout);
    leftLayout->addStretch(kVeryBigStretch);

    rightLayout = new QGraphicsLinearLayout();
    rightLayout->setOrientation(Qt::Vertical);
    rightLayout->addItem(outputsLayout);
    rightLayout->addStretch(kVeryBigStretch);

    enum 
    {
        kOuterMargin = 24
        , kTopMargin = kOuterMargin * 2
        , kItemsSpacing = 8
        , kInnerMargin = kItemsSpacing
    };

    rightLayout->setContentsMargins(kInnerMargin, kTopMargin, kOuterMargin, kOuterMargin);
    leftLayout->setContentsMargins(kOuterMargin, kTopMargin, kInnerMargin, kOuterMargin);

    QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout();
    mainLayout->setOrientation(Qt::Horizontal);
    mainLayout->addItem(leftLayout);
    mainLayout->addItem(rightLayout);
    mainLayout->setSpacing(kItemsSpacing);
    mainLayout->addStretch(kVeryBigStretch);

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

QGraphicsLayoutItem *QnIoModuleOverlayWidgetPrivate::createButton(QnIoModuleOverlayWidgetPrivate::ModelData *data)
{
    QPushButton *button = new QPushButton(trimName(data->ioConfigData.outputName));
    button->setProperty(ioPortPropertyName, data->ioConfigData.id);
    button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    button->setAutoDefault(false);
    QObject::connect(button, &QPushButton::clicked, this, &QnIoModuleOverlayWidgetPrivate::at_buttonClicked);

    Q_Q(QnIoModuleOverlayWidget);
    QGraphicsProxyWidget *buttonProxy = new QGraphicsProxyWidget(q);
    buttonProxy->setWidget(button);
    return buttonProxy;
}

QGraphicsLayoutItem *QnIoModuleOverlayWidgetPrivate::createLabel(QnIoModuleOverlayWidgetPrivate::ModelData *data)
{
    Q_Q(QnIoModuleOverlayWidget);
    GraphicsLabel *descriptionLabel = new GraphicsLabel(trimName(data->ioConfigData.inputName), q);
    descriptionLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    descriptionLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);

    return descriptionLabel;
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

    QGraphicsGridLayout * const layout = (data->ioConfigData.portType == Qn::PT_Output ? outputsLayout : inputsLayout);
    const int row = layout->rowCount();
    layout->addItem(portIdLabel, row, 0);
    layout->addItem(data->indicator, row, 1);

    QGraphicsLayoutItem * const thirdItem = (data->ioConfigData.portType == Qn::PT_Output ? 
        createButton(data) : createLabel(data));

    layout->addItem(thirdItem, row, 2);
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
    eventParams.setEventTimestamp(qnSyncTime->currentUSecsSinceEpoch());

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
