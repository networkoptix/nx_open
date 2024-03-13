// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "self.h"

#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/items/resource/web_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

#include "detail/globals_structures.h"
#include "types.h"

namespace nx::vms::client::desktop::jsapi {

class Self::Private: public CurrentSystemContextAware
{
public:
    Private(Self* q, QnWorkbenchItem* item):
        CurrentSystemContextAware(item->layout()->windowContext()),
        q(q),
        m_item(item),
        m_tab(std::make_unique<Tab>(windowContext(), item->layout()))
    {
    }

    Error setMinimalInterfaceMode(bool value);
    Error setPreventDefaultContextMenu(bool value);
    Tab* tab() const { return m_tab.get(); }
    QnWorkbenchItem* item() const { return m_item; }

private:
    Self* const q = nullptr;
    QnWorkbenchItem* const m_item = nullptr;
    std::unique_ptr<Tab> m_tab;
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
    registerTypes();
}

Self::~Self()
{
}

Error Self::setMinimalInterfaceMode(bool value)
{
    return d->setMinimalInterfaceMode(value);
}

Error Self::setPreventDefaultContextMenu(bool value)
{
    return d->setPreventDefaultContextMenu(value);
}

Tab* Self::tab() const
{
    return d->tab();
}

nx::Uuid Self::itemId() const
{
    return d->item() ? d->item()->uuid() : nx::Uuid{};
}

} // namespace nx::vms::client::desktop::jsapi
