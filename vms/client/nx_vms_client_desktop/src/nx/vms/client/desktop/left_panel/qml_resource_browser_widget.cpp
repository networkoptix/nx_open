// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_resource_browser_widget.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtWidgets/QToolTip>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/utils/qml_helpers.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_target_provider.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <utils/common/event_processors.h>

#include "private/resource_browser_wrapper_p.h"

namespace nx::vms::client::desktop {

struct QmlResourceBrowserWidget::Private
{
    QmlResourceBrowserWidget* const q;
    const QmlProperty<qreal> opacity{q, "opacity"};
    const ResourceBrowserWrapper resourceBrowser{q->windowContext(), {q, "resourceBrowser"}, q};
};

// ------------------------------------------------------------------------------------------------
// QmlResourceBrowserWidget

QmlResourceBrowserWidget::QmlResourceBrowserWidget(WindowContext* context, QWidget* parent):
    base_type(appContext()->qmlEngine(), parent),
    WindowContextAware(context),
    d(new Private{this})
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    setMouseTracking(true);

    // For semi-transparency:
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setAttribute(Qt::WA_TranslucentBackground);

    rootContext()->setContextProperty(WindowContext::kQmlPropertyName, context);

    setClearColor(Qt::transparent);
    setResizeMode(QQuickWidget::ResizeMode::SizeRootObjectToView);
    setSource(QUrl("Nx/Items/private/ResourceBrowserWrapper.qml"));

    installEventHandler(this, QEvent::Move, this, &QmlResourceBrowserWidget::positionChanged);

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
        [this]() { setSource({}); }); //< Cleanup internal stuff.
}

QmlResourceBrowserWidget::~QmlResourceBrowserWidget()
{
}

qreal QmlResourceBrowserWidget::position() const
{
    return pos().x();
}

void QmlResourceBrowserWidget::setPosition(qreal value)
{
    move(value, pos().y());
}

qreal QmlResourceBrowserWidget::opacity() const
{
    return d->opacity();
}

void QmlResourceBrowserWidget::setOpacity(qreal value)
{
    d->opacity = value;
}

menu::Parameters QmlResourceBrowserWidget::currentParameters(menu::ActionScope scope) const
{
    return d->resourceBrowser.currentParameters();
}

int QmlResourceBrowserWidget::helpTopicAt(const QPointF& pos) const
{
    return HelpTopicAccessor::helpTopicAt(rootObject(), pos);
}

} // namespace nx::vms::client::desktop
