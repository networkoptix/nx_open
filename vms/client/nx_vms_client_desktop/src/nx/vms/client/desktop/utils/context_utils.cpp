// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "context_utils.h"

#include <QtCore/QObject>
#include <QtQml/QtQml>
#include <QtWidgets/QGraphicsItem>

#include <nx/vms/client/desktop/current_system_context_aware.h>
#include <ui/graphics/view/graphics_scene.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop::utils {

WindowContext* windowContextFromObject(QObject* parent)
{
    while (parent)
    {
        if (const auto contextAware = dynamic_cast<CurrentSystemContextAware*>(parent))
            return contextAware->windowContext();

        if (const auto contextAware = dynamic_cast<WindowContextAware*>(parent))
            return contextAware->windowContext();

        if (parent->parent())
        {
            parent = parent->parent();
        }
        else
        {
            if (const auto parentItem = qobject_cast<QGraphicsItem*>(parent))
                parent = parentItem->scene();
            else if (const auto parentWidget = qobject_cast<QWidget*>(parent))
                parent = parentWidget->parentWidget();
            else
                parent = nullptr;
        }
    }

    NX_ASSERT(parent,
        "Invalid parent. TODO: implement lazy initialization.");
    return nullptr;
}

} // namespace nx::vms::client::desktop::utils
