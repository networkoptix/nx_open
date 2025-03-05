// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "io_module_overlay_widget.h"

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtWidgets/QGraphicsLinearLayout>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/camera/iomodule/io_module_monitor.h>
#include <nx/vms/client/core/io_ports/io_ports_compatibility_interface.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_parameters.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include "io_module_form_overlay_contents.h"
#include "io_module_grid_overlay_contents.h"

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {

constexpr int kReconnectDelayMs = 1000 * 5;
constexpr int kCommandExecuteTimeoutMs = 1000 * 6;

} // namespace

class QnIoModuleOverlayWidgetPrivate: public QObject
{
    using base_type = QObject;
    using Style = QnIoModuleOverlayWidget::Style;

    Q_DISABLE_COPY(QnIoModuleOverlayWidgetPrivate)
    Q_DECLARE_PUBLIC(QnIoModuleOverlayWidget)
    Q_DECLARE_TR_FUNCTIONS(QnIoModuleOverlayWidgetPrivate)

public:
    QnIoModuleOverlayWidget* const q_ptr;
    QScopedPointer<QnIoModuleOverlayContents> contents;

    QnVirtualCameraResourcePtr module;
    const core::IOModuleMonitorPtr monitor;
    bool userInputEnabled = false;

    struct StateData
    {
        QnIOPortData config;            //< port configuration
        QnIOStateData state;            //< current port state
        bool requestIsInProgress = false;
    };

    QnIOPortDataList ports;
    QMap<QString, StateData> states;
    QGraphicsLinearLayout* const layout = nullptr;
    QnIoModuleOverlayWidget::Style overlayStyle = QnIoModuleOverlayWidget::Style();

    QnIoModuleOverlayWidgetPrivate(
        const core::IOModuleMonitorPtr& monitor,
        QnIoModuleOverlayWidget* widget);

    void setContents(QnIoModuleOverlayContents* newContents);

    void initIOModule(const QnVirtualCameraResourcePtr& newModule);
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
};

/*
QnIoModuleOverlayWidgetPrivate
*/

QnIoModuleOverlayWidgetPrivate::QnIoModuleOverlayWidgetPrivate(
    const core::IOModuleMonitorPtr& monitor,
    QnIoModuleOverlayWidget* widget)
    :
    base_type(widget),
    q_ptr(widget),
    monitor(monitor),
    layout(new QGraphicsLinearLayout(Qt::Vertical, widget))
{
    widget->setAutoFillBackground(true);

    updateOverlayStyle();
}

void QnIoModuleOverlayWidgetPrivate::setContents(QnIoModuleOverlayContents* newContents)
{
    NX_ASSERT(newContents);
    contents.reset(newContents);

    layout->addItem(newContents);

    connect(contents.get(), &QnIoModuleOverlayContents::userClicked,
        this, &QnIoModuleOverlayWidgetPrivate::toggleState);

    updateContents();
}

void QnIoModuleOverlayWidgetPrivate::updateOverlayStyle()
{
    auto style = overlayStyle;
    if (module)
    {
        style = nx::reflect::fromString<QnIoModuleOverlayWidget::Style>(
            module->getProperty(ResourcePropertyKey::kIoOverlayStyle).toStdString(), {});
    }

    bool needToCreateNewContents = style != overlayStyle || !contents;
    if (!needToCreateNewContents)
        return;

    overlayStyle = style;

    switch (overlayStyle)
    {
        case Style::tile:
            setContents(new QnIoModuleGridOverlayContents());
            break;

        case Style::form:
        default:
            setContents(new QnIoModuleFormOverlayContents());
            break;
    }
}

void QnIoModuleOverlayWidgetPrivate::initIOModule(const QnVirtualCameraResourcePtr& newModule)
{
    Q_Q(QnIoModuleOverlayWidget);

    if (module)
        module->disconnect(this);

    module = newModule;
    if (module && !module->isIOModule())
    {
        NX_ASSERT(false, "Must be I/O module!");
        module.clear();
    }

    states.clear();

    if (!module)
    {
        setPorts({});
        q->setEnabled(false);
        return;
    }

    connect(monitor.get(), &core::IOModuleMonitor::connectionOpened,
        this, &QnIoModuleOverlayWidgetPrivate::at_connectionOpened);
    connect(monitor.get(), &core::IOModuleMonitor::connectionClosed,
        this, &QnIoModuleOverlayWidgetPrivate::at_connectionClosed);
    connect(monitor.get(), &core::IOModuleMonitor::ioStateChanged,
        this, &QnIoModuleOverlayWidgetPrivate::at_stateChanged);

    connect(module.get(), &QnResource::propertyChanged,
        this, &QnIoModuleOverlayWidgetPrivate::at_cameraPropertyChanged);
    connect(module.get(), &QnResource::statusChanged,
        this, &QnIoModuleOverlayWidgetPrivate::at_cameraStatusChanged);

    updateOverlayStyle();

    setPorts(module->ioPortDescriptions());
}

void QnIoModuleOverlayWidgetPrivate::setPorts(const QnIOPortDataList& newPorts)
{
    ports = newPorts;
    std::sort(ports.begin(), ports.end(),
        [](const QnIOPortData& left, const QnIOPortData& right)
        {
            return nx::utils::naturalStringCompare(
                left.getName(),
                right.getName(),
                Qt::CaseInsensitive) < 0;
        });

    for (const auto& port : ports)
    {
        auto& item = states[port.id];
        item.config = port;
        item.state.id = port.id;
    }

    if (monitor && !monitor->connectionIsOpened())
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
        setPorts(module->ioPortDescriptions());
}

void QnIoModuleOverlayWidgetPrivate::openConnection()
{
    if (!monitor->open())
        at_connectionClosed();
}

void QnIoModuleOverlayWidgetPrivate::at_cameraStatusChanged(const QnResourcePtr& resource)
{
    if (resource->getStatus() < nx::vms::api::ResourceStatus::online)
        at_connectionClosed();
}

void QnIoModuleOverlayWidgetPrivate::at_cameraPropertyChanged(const QnResourcePtr& resource, const QString& key)
{
    if (resource != module)
        return;

    if (key == ResourcePropertyKey::kIoSettings)
    {
        setPorts(module->ioPortDescriptions());
    }
    else if (key == ResourcePropertyKey::kIoOverlayStyle)
    {
        updateOverlayStyle();
    }
}

void QnIoModuleOverlayWidgetPrivate::at_connectionOpened()
{
    Q_Q(QnIoModuleOverlayWidget);
    q->setEnabled(true);
}

void QnIoModuleOverlayWidgetPrivate::at_connectionClosed()
{
    Q_Q(QnIoModuleOverlayWidget);

    q->setEnabled(false);

    NX_VERBOSE(this, "Connection closed, state is reset");

    // Resets states since server initially sends only active states after restart.
    for (auto& item: states)
    {
        item.state.isActive = false;
        item.state.timestampUs = -1;
        item.requestIsInProgress = false;
        contents->stateChanged(item.config, item.state);
    }

    executeDelayedParented([this](){ openConnection(); }, kReconnectDelayMs, this);
}

void QnIoModuleOverlayWidgetPrivate::at_stateChanged(const QnIOStateData& value)
{
    auto it = states.find(value.id);
    if (it == states.end())
        return;

    it->state = value;
    it->requestIsInProgress = false;
    NX_VERBOSE(this, "I/O port %1 state changed: %2", value.id, value.isActive);

    contents->stateChanged(it->config, it->state);
}

void QnIoModuleOverlayWidgetPrivate::toggleState(const QString& port)
{
    Q_Q(QnIoModuleOverlayWidget);

    if (port.isEmpty() || !userInputEnabled)
        return;

    auto it = states.find(port);
    if (it == states.end())
        return;

    /* Ignore if we didn't receive response from previous toggle yet: */
    if (it->requestIsInProgress)
        return;

    /* Send fake early state change to make UI look more responsive: */
    auto newState = it->state;
    newState.isActive = !newState.isActive;
    contents->stateChanged(it->config, newState);

    const auto systemContext = module->systemContext()->as<SystemContext>();
    if (!NX_ASSERT(systemContext))
        return;

    if (!systemContext->connection() || !module->getParentServer())
    {
        NX_WARNING(this,
            "Can't delivery event to the target server %1. Not found", module->getParentId());
        return;
    }

    if (!systemContext->ioPortsInterface())
        return;

    auto callback = [this, port, q](bool success)
        {
            auto it = states.find(port);
            if (it == states.end())
                return;

            if (!it->requestIsInProgress)
                return;

            NX_VERBOSE(this, "I/O port %1 change request is successful: %2", port, success);

            it->requestIsInProgress = false;

            if (!success)
            {
                const auto portName = it->config.getName();
                const auto message = it->state.isActive
                    ? tr("Failed to turn off I/O port %1")
                    : tr("Failed to turn on I/O port %1");

                QnSessionAwareMessageBox::warning(
                    q->windowContext()->mainWindowWidget(), message.arg(portName));
            }
        };

    it->requestIsInProgress = true; //< Ensures setting it up before potential callback call.

    bool success = systemContext->ioPortsInterface()->setIoPortState(
        systemContext->getSessionTokenHelper(),
        module,
        it->config.id,
        /*isActive*/ newState.isActive,
        /*autoResetTimeout*/ std::chrono::milliseconds(it->config.autoResetTimeoutMs),
        /*targetLockResolutionData*/ {},
        callback);

    NX_VERBOSE(this, "I/O port %1 change request attempt, sending is successful: %2",
        port, success);

    if (!success)
        it->requestIsInProgress = false;
}

/*
QnIoModuleOverlayWidget
*/

QnIoModuleOverlayWidget::QnIoModuleOverlayWidget(
    const QnVirtualCameraResourcePtr& module,
    const core::IOModuleMonitorPtr& monitor,
    WindowContext* windowContext,
    QGraphicsWidget* parent)
    :
    base_type(parent),
    WindowContextAware(windowContext),
    d_ptr(new QnIoModuleOverlayWidgetPrivate(monitor, this))
{
    Q_D(QnIoModuleOverlayWidget);
    d->initIOModule(module);
    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dark6"));
}

QnIoModuleOverlayWidget::~QnIoModuleOverlayWidget()
{
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
