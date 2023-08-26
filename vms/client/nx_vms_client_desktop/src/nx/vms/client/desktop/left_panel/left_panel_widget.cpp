// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "left_panel_widget.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QToolTip>

#include <client/client_globals.h>
#include <client_core/client_core_module.h>
#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/utils/qml_helpers.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_target_provider.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

#include "private/resource_browser_wrapper_p.h"

namespace nx::vms::client::desktop {

class LeftPanelWidget::Private: public QObject
{
    LeftPanelWidget* const q;
    class StateDelegate;

public:
    Private(LeftPanelWidget* main);
    void initialize();

    const QmlProperty<QQuickItem*> panel{q, "panel"};

    const QmlProperty<RightPanel::Tab> currentTab{&panel, "currentTab"};
    const QmlProperty<bool> resizeEnabled{&panel, "resizeEnabled"};
    const QmlProperty<qreal> maximumWidth{&panel, "maximumWidth"};
    const QmlProperty<qreal> height{q, "height"};
    const QmlProperty<qreal> opacity{q, "opacity"};
    const ResourceBrowserWrapper resourceBrowser{q->context(), QmlProperty<QQuickItem*>(&panel, "resourceBrowser"), q};

    const QmlProperty<QQuickItem*> openCloseButton{q, "openCloseButton"};
    const QmlProperty<bool> openCloseButtonHovered{&openCloseButton, "hovered"};
    const QmlProperty<bool> openCloseButtonChecked{&openCloseButton, "checked"};

    void setCurrentTab(RightPanel::Tab value)
    {
        invokeQmlMethod<void>(panel, "setCurrentTab", (int) value);
    }

    QString toolTip() const
    {
        return openCloseButtonHovered()
            ? q->action(ui::action::ToggleLeftPanelAction)->text()
            : QString();
    }
};

// ------------------------------------------------------------------------------------------------
// LeftPanelWidget

LeftPanelWidget::LeftPanelWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(qnClientCoreModule->mainQmlEngine(), parent),
    QnWorkbenchContextAware(context),
    d(new Private(this))
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    setMouseTracking(true);

    // For semi-transparency:
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setAttribute(Qt::WA_TranslucentBackground);

    rootContext()->setContextProperty(
        QnWorkbenchContextAware::kQmlWorkbenchContextPropertyName, this->context());

    rootContext()->setContextProperty("maxTextureSize",
        QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE));

    connect(this, &QQuickWidget::statusChanged, d.get(),
        [this]()
        {
            if (status() == QQuickWidget::Ready)
                d->initialize();
        });

    setClearColor(Qt::transparent);
    setResizeMode(QQuickWidget::ResizeMode::SizeViewToRootObject);
    setSource(QUrl("Nx/LeftPanel/private/LeftPanelWrapper.qml"));

    // Avoid shortcut ambiguity with Welcome Screen and other possible places.
    installEventHandler(this, {QEvent::Show, QEvent::Hide}, this,
        [this]() { action(ui::action::SearchResourcesAction)->setEnabled(isVisible()); });

    d->openCloseButtonChecked.connectNotifySignal([this]() { setToolTip(d->toolTip()); });

    d->openCloseButtonHovered.connectNotifySignal(
        [this]()
        {
            if (!d->openCloseButtonHovered)
                QToolTip::hideText();

            setToolTip(d->toolTip());
        });

    action(ui::action::SearchResourcesAction)->setEnabled(false);

    installEventHandler(this, QEvent::Move, this, &LeftPanelWidget::positionChanged);

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, d.get(),
        [this]() { setSource({}); }); //< Cleanup internal stuff.
}

LeftPanelWidget::~LeftPanelWidget()
{
}

qreal LeftPanelWidget::position() const
{
    return pos().x();
}

void LeftPanelWidget::setPosition(qreal value)
{
    move(value, pos().y());
}

void LeftPanelWidget::setHeight(qreal value)
{
    d->height = value;
}

void LeftPanelWidget::setMaximumAllowedWidth(qreal value)
{
    d->maximumWidth = value;
}

void LeftPanelWidget::setResizeEnabled(bool value)
{
    d->resizeEnabled = value;
}

qreal LeftPanelWidget::opacity() const
{
    return d->opacity();
}

void LeftPanelWidget::setOpacity(qreal value)
{
    d->opacity = value;
}

ui::action::ActionScope LeftPanelWidget::currentScope() const
{
    return d->currentTab == RightPanel::Tab::resources
        ? ui::action::TreeScope
        : ui::action::InvalidScope;
}

ui::action::Parameters LeftPanelWidget::currentParameters(ui::action::ActionScope scope) const
{
    return scope == ui::action::TreeScope
        ? d->resourceBrowser.currentParameters()
        : ui::action::Parameters();
}

QQuickItem* LeftPanelWidget::openCloseButton() const
{
    return d->openCloseButton();
}

int LeftPanelWidget::helpTopicAt(const QPointF& pos) const
{
    return HelpTopicAccessor::helpTopicAt(rootObject(), pos);
}

// ------------------------------------------------------------------------------------------------
// LeftPanelWidget::Private

LeftPanelWidget::Private::Private(LeftPanelWidget* main):
    q(main)
{
}

void LeftPanelWidget::Private::initialize()
{
    static constexpr std::pair<ui::action::IDType, RightPanel::Tab> actions[] = {
        {ui::action::ResourcesTabAction, RightPanel::Tab::resources},
        {ui::action::MotionTabAction, RightPanel::Tab::motion},
        {ui::action::BookmarksTabAction, RightPanel::Tab::bookmarks},
        {ui::action::EventsTabAction, RightPanel::Tab::events},
        {ui::action::ObjectsTabAction, RightPanel::Tab::analytics}};

    const auto setTabFunction =
        [this](RightPanel::Tab tab)
        {
            return [this, tab]() { setCurrentTab(tab); };
        };

    // Connect actions to tab activation functions and specify action shortcut keys for tooltips.
    for (const auto& [actionId, tabId]: actions)
    {
        const auto action = q->action(actionId);

        connect(action, &QAction::triggered, this, setTabFunction(tabId));

        invokeQmlMethod<void>(panel,
            "setShortcutHintText", (int) tabId, action->shortcut().toString());
    }

    const auto updateOpenCloseButton =
        [this]()
        {
            openCloseButtonChecked = q->action(ui::action::ToggleLeftPanelAction)->isChecked();
        };

    openCloseButtonChecked.connectNotifySignal(
        [this]()
        {
            q->action(ui::action::ToggleLeftPanelAction)->setChecked(openCloseButtonChecked);
        });

    connect(q->action(ui::action::ToggleLeftPanelAction), &QAction::toggled, this,
        updateOpenCloseButton);

    updateOpenCloseButton();
}

} // namespace nx::vms::client::desktop
