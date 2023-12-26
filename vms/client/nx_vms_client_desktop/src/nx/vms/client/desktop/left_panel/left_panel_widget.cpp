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
#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/utils/qml_helpers.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_target_provider.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/graphics/opengl/gl_functions.h>
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
    const ResourceBrowserWrapper resourceBrowser{q->windowContext(),
        QmlProperty<QQuickItem*>(&panel, "resourceBrowser"), q};

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
            ? q->action(menu::ToggleLeftPanelAction)->text()
            : QString();
    }
};

// ------------------------------------------------------------------------------------------------
// LeftPanelWidget

LeftPanelWidget::LeftPanelWidget(WindowContext* context, QWidget* parent):
    base_type(appContext()->qmlEngine(), parent),
    WindowContextAware(context),
    d(new Private(this))
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    setMouseTracking(true);

    // For semi-transparency:
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setAttribute(Qt::WA_TranslucentBackground);

    rootContext()->setContextProperty(WindowContext::kQmlPropertyName, context);

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
        [this]() { action(menu::SearchResourcesAction)->setEnabled(isVisible()); });

    d->openCloseButtonChecked.connectNotifySignal([this]() { setToolTip(d->toolTip()); });

    d->openCloseButtonHovered.connectNotifySignal(
        [this]()
        {
            if (!d->openCloseButtonHovered)
                QToolTip::hideText();

            setToolTip(d->toolTip());
        });

    action(menu::SearchResourcesAction)->setEnabled(false);

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

menu::ActionScope LeftPanelWidget::currentScope() const
{
    return d->currentTab == RightPanel::Tab::resources
        ? menu::TreeScope
        : menu::InvalidScope;
}

menu::Parameters LeftPanelWidget::currentParameters(menu::ActionScope scope) const
{
    return scope == menu::TreeScope
        ? d->resourceBrowser.currentParameters()
        : menu::Parameters();
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
    static constexpr std::pair<menu::IDType, RightPanel::Tab> actions[] = {
        {menu::ResourcesTabAction, RightPanel::Tab::resources},
        {menu::MotionTabAction, RightPanel::Tab::motion},
        {menu::BookmarksTabAction, RightPanel::Tab::bookmarks},
        {menu::EventsTabAction, RightPanel::Tab::events},
        {menu::ObjectsTabAction, RightPanel::Tab::analytics}};

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
            openCloseButtonChecked = q->action(menu::ToggleLeftPanelAction)->isChecked();
        };

    openCloseButtonChecked.connectNotifySignal(
        [this]()
        {
            q->action(menu::ToggleLeftPanelAction)->setChecked(openCloseButtonChecked);
        });

    connect(q->action(menu::ToggleLeftPanelAction), &QAction::toggled, this,
        updateOpenCloseButton);

    updateOpenCloseButton();
}

} // namespace nx::vms::client::desktop
