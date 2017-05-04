#include "io_module_overlay_widget.h"

#include <QtCore/QElapsedTimer>

#include <QtWidgets/QGraphicsLinearLayout>

#include "io_module_form_overlay_contents.h"
#include "io_module_grid_overlay_contents.h"

#include <client_core/connection_context_aware.h>

#include <common/common_module.h>

#include <api/app_server_connection.h>
#include <core/resource/camera_resource.h>
#include <camera/iomodule/iomodule_monitor.h>
#include <business/business_event_parameters.h>
#include <business/business_action_parameters.h>
#include <business/actions/camera_output_business_action.h>
#include <nx/fusion/model_functions.h>
#include <utils/common/connective.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

namespace {

constexpr int kReconnectDelayMs = 1000 * 5;
constexpr int kCommandExecuteTimeoutMs = 1000 * 6;
constexpr int kStateCheckIntervalMs = 1000;

} // namespace

class QnIoModuleOverlayWidgetPrivate: public Connective<QObject>,
    public QnConnectionContextAware
{
    using base_type = Connective<QObject>;

    Q_DISABLE_COPY(QnIoModuleOverlayWidgetPrivate)
    Q_DECLARE_PUBLIC(QnIoModuleOverlayWidget)
    Q_DECLARE_TR_FUNCTIONS(QnIoModuleOverlayWidgetPrivate)

public:
    QnIoModuleOverlayWidget* const q_ptr;
    QScopedPointer<QnIoModuleOverlayContents> contents;

    QnVirtualCameraResourcePtr module;
    QnIOModuleMonitorPtr monitor;
    bool connectionOpened = false;
    bool userInputEnabled = false;

    struct StateData
    {
        QnIOPortData config;            //< port configuration
        QnIOStateData state;            //< current port state
        QElapsedTimer stateChangeTimer; //< state change timout timer
    };

    QnIOPortDataList ports;
    QMap<QString, StateData> states;
    QTimer* const timer;
    QGraphicsLinearLayout* const layout;
    QnIoModuleOverlayWidget::Style overlayStyle;

    QnIoModuleOverlayWidgetPrivate(QnIoModuleOverlayWidget* widget);

    void setContents(QnIoModuleOverlayContents* newContents);

    void setIOModule(const QnVirtualCameraResourcePtr& newModule);
    void setPorts(const QnIOPortDataList& newPorts);

    void updateContents();
    void updateOverlayStyle();

    void openConnection();
    void toggleState(const QString& port);

    void at_cameraStatusChanged(const QnResourcePtr& resource);
    void at_cameraPropertyChanged(const QnResourcePtr& resource, const QString& key);
    void at_connectionOpened();
    void at_connectionClosed();
    void at_stateChanged(const QnIOStateData& value);
    void at_timerTimeout();
};

/*
QnIoModuleOverlayWidgetPrivate
*/

QnIoModuleOverlayWidgetPrivate::QnIoModuleOverlayWidgetPrivate(QnIoModuleOverlayWidget* widget):
    base_type(widget),
    q_ptr(widget),
    timer(new QTimer(this)),
    layout(new QGraphicsLinearLayout(Qt::Vertical, widget)),
    overlayStyle(QnIoModuleOverlayWidget::Style::Default)
{
    widget->setAutoFillBackground(true);

    connect(timer, &QTimer::timeout, this, &QnIoModuleOverlayWidgetPrivate::at_timerTimeout);
    timer->setInterval(kStateCheckIntervalMs);

    updateOverlayStyle();
}

void QnIoModuleOverlayWidgetPrivate::setContents(QnIoModuleOverlayContents* newContents)
{
    NX_ASSERT(newContents);
    contents.reset(newContents);

    layout->addItem(newContents);

    connect(contents, &QnIoModuleOverlayContents::userClicked,
        this, &QnIoModuleOverlayWidgetPrivate::toggleState);

    updateContents();
}

void QnIoModuleOverlayWidgetPrivate::updateOverlayStyle()
{
    auto style = overlayStyle;
    if (module)
    {
        style = QnLexical::deserialized<QnIoModuleOverlayWidget::Style>(
            module->getProperty(Qn::IO_OVERLAY_STYLE_PARAM_NAME),
            QnIoModuleOverlayWidget::Style::Default);
    }

    bool needToCreateNewContents = style != overlayStyle || !contents;
    if (!needToCreateNewContents)
        return;

    overlayStyle = style;

    switch (overlayStyle)
    {
        case QnIoModuleOverlayWidget::Style::Tile:
            setContents(new QnIoModuleGridOverlayContents());
            break;

        case QnIoModuleOverlayWidget::Style::Form:
        default:
            setContents(new QnIoModuleFormOverlayContents());
            break;
    }
}

void QnIoModuleOverlayWidgetPrivate::setIOModule(const QnVirtualCameraResourcePtr& newModule)
{
    Q_Q(QnIoModuleOverlayWidget);
    timer->stop();

    if (module)
        module->disconnect(this);

    module = newModule;
    if (module && !module->isIOModule())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Must be I/O module!");
        module.clear();
    }

    states.clear();

    if (!module)
    {
        monitor.reset();
        setPorts({});
        q->setEnabled(false);
        return;
    }

    monitor.reset(new QnIOModuleMonitor(module));

    connect(monitor, &QnIOModuleMonitor::connectionOpened,
        this, &QnIoModuleOverlayWidgetPrivate::at_connectionOpened);
    connect(monitor, &QnIOModuleMonitor::connectionClosed,
        this, &QnIoModuleOverlayWidgetPrivate::at_connectionClosed);
    connect(monitor, &QnIOModuleMonitor::ioStateChanged,
        this, &QnIoModuleOverlayWidgetPrivate::at_stateChanged);

    connect(module, &QnResource::propertyChanged,
        this, &QnIoModuleOverlayWidgetPrivate::at_cameraPropertyChanged);
    connect(module, &QnResource::statusChanged,
        this, &QnIoModuleOverlayWidgetPrivate::at_cameraStatusChanged);

    updateOverlayStyle();

    setPorts(module->getIOPorts());
    timer->start();
}

void QnIoModuleOverlayWidgetPrivate::setPorts(const QnIOPortDataList& newPorts)
{
    ports = newPorts;

    for (const auto& port : ports)
    {
        auto& item = states[port.id];
        item.config = port;
        item.state.id = port.id;
    }

    if (monitor && !connectionOpened)
        openConnection();

    contents->portsChanged(ports, userInputEnabled);

    for (const auto& item: states)
        contents->stateChanged(item.config, item.state);
}

void QnIoModuleOverlayWidgetPrivate::updateContents()
{
    /* This is a "hard-reset" of widgets displaying I/O ports
     * It is called when colors or user input enabled state is changed
     * This normally should not happen often when I/O module overlay is open */
    if (module)
        setPorts(module->getIOPorts());
}

void QnIoModuleOverlayWidgetPrivate::openConnection()
{
    if (!monitor->open())
        at_connectionClosed();
}

void QnIoModuleOverlayWidgetPrivate::at_cameraStatusChanged(const QnResourcePtr& resource)
{
    if (resource->getStatus() < Qn::Online)
        at_connectionClosed();
}

void QnIoModuleOverlayWidgetPrivate::at_cameraPropertyChanged(const QnResourcePtr& resource, const QString& key)
{
    if (resource != module)
        return;

    if (key == Qn::IO_SETTINGS_PARAM_NAME)
    {
        setPorts(module->getIOPorts());
    }
    else if (key == Qn::IO_OVERLAY_STYLE_PARAM_NAME)
    {
        updateOverlayStyle();
    }
}

void QnIoModuleOverlayWidgetPrivate::at_connectionOpened()
{
    Q_Q(QnIoModuleOverlayWidget);
    connectionOpened = true;
    q->setEnabled(true);
}

void QnIoModuleOverlayWidgetPrivate::at_connectionClosed()
{
    Q_Q(QnIoModuleOverlayWidget);

    q->setEnabled(false);
    connectionOpened = false;

    for (auto& item: states)
        item.stateChangeTimer.invalidate();

    executeDelayedParented([this](){ openConnection(); }, kReconnectDelayMs, this);
}

void QnIoModuleOverlayWidgetPrivate::at_stateChanged(const QnIOStateData& value)
{
    auto it = states.find(value.id);
    if (it == states.end())
        return;

    it->state = value;
    it->stateChangeTimer.invalidate();

    contents->stateChanged(it->config, it->state);
}

void QnIoModuleOverlayWidgetPrivate::toggleState(const QString& port)
{
    if (port.isEmpty() || !userInputEnabled)
        return;

    auto it = states.find(port);
    if (it == states.end())
        return;

    /* Ignore if we didn't receive response from previous toggle yet: */
    if (it->stateChangeTimer.isValid())
        return;

    /* Send fake early state change to make UI look more responsive: */
    auto newState = it->state;
    newState.isActive = !newState.isActive;
    contents->stateChanged(it->config, newState);

    QnBusinessEventParameters eventParams;
    eventParams.eventTimestampUsec = qnSyncTime->currentUSecsSinceEpoch();

    QnBusinessActionParameters params;
    params.relayOutputId = it->config.id;
    params.durationMs = it->config.autoResetTimeoutMs;

    QnCameraOutputBusinessActionPtr action(new QnCameraOutputBusinessAction(eventParams));
    action->setParams(params);
    action->setResources({ module->getId() });
    action->setToggleState(it->state.isActive ? QnBusiness::InactiveState : QnBusiness::ActiveState);

    ec2::AbstractECConnectionPtr connection = commonModule()->ec2Connection();
    // we are not interested in client->server transport error code because of real port checking by timer
    if (connection)
        connection->getBusinessEventManager(Qn::kSystemAccess)->sendBusinessAction(action, module->getParentId(), this, [] {});

    it->stateChangeTimer.restart();
}

void QnIoModuleOverlayWidgetPrivate::at_timerTimeout()
{
    for (auto& item: states)
    {
        if (!item.stateChangeTimer.isValid())
            continue;

        //TODO: Probably rewrite this logic

        /* If I/O port toggle is confirmed by the monitor, stateChangeTimer is invalidated. */
        /* If stateChangeTimer is valid and expired, I/O port toggle was unsuccessful: */
        if (item.stateChangeTimer.hasExpired(kCommandExecuteTimeoutMs))
        {
            contents->stateChanged(item.config, item.state);

            item.stateChangeTimer.invalidate();

            const auto portName = item.config.getName();
            const auto message = item.state.isActive
                ? tr("Failed to turn off I/O port %1")
                : tr("Failed to turn on I/O port %1");

            QnMessageBox::warning(nullptr, message.arg(portName));
        }
    }
}

/*
QnIoModuleOverlayWidget
*/

QnIoModuleOverlayWidget::QnIoModuleOverlayWidget(QGraphicsWidget* parent):
    base_type(parent),
    d_ptr(new QnIoModuleOverlayWidgetPrivate(this))
{
}

QnIoModuleOverlayWidget::~QnIoModuleOverlayWidget()
{
}

void QnIoModuleOverlayWidget::setIOModule(const QnVirtualCameraResourcePtr& module)
{
    Q_D(QnIoModuleOverlayWidget);
    d->setIOModule(module);
}

QnIoModuleOverlayWidget::Style QnIoModuleOverlayWidget::overlayStyle() const
{
    Q_D(const QnIoModuleOverlayWidget);
    return d->overlayStyle;
}

bool QnIoModuleOverlayWidget::userInputEnabled() const
{
    Q_D(const QnIoModuleOverlayWidget);
    return d->userInputEnabled;
}

void QnIoModuleOverlayWidget::setUserInputEnabled(bool value)
{
    Q_D(QnIoModuleOverlayWidget);
    if (d->userInputEnabled == value)
        return;

    d->userInputEnabled = value;
    d->updateContents();
}

/*
QnIoModuleOverlayContents
*/

QnIoModuleOverlayContents::QnIoModuleOverlayContents():
    base_type()
{
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
}

QnIoModuleOverlayContents::~QnIoModuleOverlayContents()
{
}

QnIoModuleOverlayWidget* QnIoModuleOverlayContents::overlayWidget() const
{
    return qgraphicsitem_cast<QnIoModuleOverlayWidget*>(parentItem());
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnIoModuleOverlayWidget, Style,
    (QnIoModuleOverlayWidget::Style::Form, "Form")
    (QnIoModuleOverlayWidget::Style::Tile, "Tile")
)
