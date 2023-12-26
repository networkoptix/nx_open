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
#include <client_core/client_core_module.h>
#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/utils/qml_helpers.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_target_provider.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

#include "private/resource_browser_wrapper_p.h"

namespace nx::vms::client::desktop {

struct QmlResourceBrowserWidget::Private
{
    QmlResourceBrowserWidget* const q;
    const QmlProperty<qreal> opacity{q, "opacity"};
    const ResourceBrowserWrapper resourceBrowser{q->context(), {q, "resourceBrowser"}, q};
};

// ------------------------------------------------------------------------------------------------
// QmlResourceBrowserWidget

QmlResourceBrowserWidget::QmlResourceBrowserWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(qnClientCoreModule->mainQmlEngine(), parent),
    QnWorkbenchContextAware(context),
    d(new Private{this})
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    setMouseTracking(true);

    // For semi-transparency:
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setAttribute(Qt::WA_TranslucentBackground);

    rootContext()->setContextProperty(
        QnWorkbenchContextAware::kQmlWorkbenchContextPropertyName, this->context());

    setClearColor(Qt::transparent);
    setResizeMode(QQuickWidget::ResizeMode::SizeRootObjectToView);
    setSource(QUrl("Nx/LeftPanel/private/ResourceBrowserWrapper.qml"));

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

ui::action::Parameters QmlResourceBrowserWidget::currentParameters(ui::action::ActionScope scope) const
{
    return d->resourceBrowser.currentParameters();
}

int QmlResourceBrowserWidget::helpTopicAt(const QPointF& pos) const
{
    return HelpTopicAccessor::helpTopicAt(rootObject(), pos);
}

} // namespace nx::vms::client::desktop
