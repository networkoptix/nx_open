// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context_aware_attached.h"

#include <QtQml/QQmlEngine>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>

namespace nx::vms::client::desktop {

WindowContextAttached::WindowContextAttached(WindowContext* context):
    m_windowContext(context)
{
    if (!NX_ASSERT(context))
        return;

    connect(context, &WindowContext::beforeSystemChanged,
        this, &WindowContextAttached::beforeSystemChanged);
    connect(context, &WindowContext::systemChanged,
        this, &WindowContextAttached::systemChanged);
    connect(context, &WindowContext::userIdChanged,
        this, &WindowContextAttached::userIdChanged);
}

void WindowContextAttached::triggerAction(menu::IDType actionId)
{
    if (NX_ASSERT(m_windowContext))
        m_windowContext->menu()->trigger(actionId);
}

WindowContextAttached* WindowContextAwareHelper::qmlAttachedProperties(QObject* object)
{
    return new WindowContextAttached(WindowContext::fromQmlContext(object));
}

void WindowContextAwareHelper::registerQmlType()
{
    qmlRegisterType<WindowContextAwareHelper>("nx.vms.client.desktop", 1, 0, "WindowContextAware");

    qmlRegisterUncreatableMetaObject(menu::staticMetaObject, "nx.vms.client.desktop", 1, 0,
        "GlobalActions", "GlobalActions is a namespace");
}

} // namespace nx::vms::client::desktop
