// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "self.h"

#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/items/resource/web_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

#include "detail/globals_structures.h"
#include "detail/helpers.h"

namespace nx::vms::client::desktop::jsapi {

class Self::Private: public CurrentSystemContextAware
{
public:
    Private(Self* q, QnWorkbenchItem* item):
        CurrentSystemContextAware(item->layout()->windowContext()),
        q(q),
        m_item(item)
    {
    }

    /**
     * @addtogroup vms-self
     * Contains methods to control the current web-page widget.
     * @{
    */

    /**
     * Sets the web-page widget minimal interface mode. In this mode only the close button is
     * visible.
     */
    Error setMinimalInterfaceMode(bool value);

    /**
     * Prevents showing the default context menu.
     */
    Error setPreventDefaultContextMenu(bool value);

    /** @} */ // group self

private:
    Self* const q = nullptr;
    QnWorkbenchItem* const m_item = nullptr;
};

Error Self::Private::setMinimalInterfaceMode(bool value)
{
    if (!m_item)
        return Error::failed();

    const auto resourceWidget = context()->display()->widget(m_item);
    const auto webWidget = dynamic_cast<QnWebResourceWidget*>(resourceWidget);

    if (!webWidget)
        return Error::failed();

    webWidget->setMinimalTitleBarMode(value);
    return Error::success();
}

Error Self::Private::setPreventDefaultContextMenu(bool value)
{
    if (!m_item)
        return Error::failed();

    const auto resourceWidget = context()->display()->widget(m_item);
    const auto webWidget = dynamic_cast<QnWebResourceWidget*>(resourceWidget);

    if (!webWidget)
        return Error::failed();

    webWidget->setPreventDefaultContextMenu(value);
    return Error::success();
}

Self::Self(QnWorkbenchItem* item, QObject* parent):
    QObject(parent),
    d(new Private(this, item))
{
}

Self::~Self()
{
}

QJsonObject Self::setMinimalInterfaceMode(bool value)
{
    return detail::toJsonObject(d->setMinimalInterfaceMode(value));
}

QJsonObject Self::setPreventDefaultContextMenu(bool value)
{
    return detail::toJsonObject(d->setPreventDefaultContextMenu(value));
}

} // namespace nx::vms::client::desktop::jsapi
