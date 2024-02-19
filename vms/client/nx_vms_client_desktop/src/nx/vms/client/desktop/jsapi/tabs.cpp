// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tabs.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/jsapi/detail/helpers.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/delayed.h>

#include "detail/globals_structures.h"
#include "tab.h"

namespace nx::vms::client::desktop::jsapi {

struct Tabs::Private:
    public QObject,
    public WindowContextAware
{
    const QPointer<Tabs> q = nullptr;
    std::shared_ptr<Tab> current;
    QMap<nx::Uuid, std::shared_ptr<Tab>> tabs;

    Private(Tabs* q, WindowContext* context);
    Tab* addIfNeeded(QnWorkbenchLayout* layout);

    Error setCurrent(Tab* tab);
    Tab* add(const QString& name);
    Error remove(Tab* tab);
};

Tabs::Private::Private(Tabs* q, WindowContext* context):
    WindowContextAware(context),
    q(q)
{
    auto updateTabs =
        [=]()
        {
            for (const auto& layout: workbench()->layouts())
                addIfNeeded(layout.get()); //< Layouts removed when destroyed.

            emit q->tabsChanged();
        };

    connect(workbench(), &Workbench::layoutsChanged, this, updateTabs);

    updateTabs();

    auto updateCurrent =
        [=]
        {
            current = tabs.value(workbench()->currentLayout()->resourceId());
            emit q->currentChanged();
        };

    connect(workbench(), &Workbench::currentLayoutChanged, this, updateCurrent);

    updateCurrent();
}

Tab* Tabs::Private::addIfNeeded(QnWorkbenchLayout* layout)
{
    if (const auto it = tabs.find(layout->resourceId()); it != tabs.end())
        return it.value().get();

    const auto tab = std::make_shared<Tab>(windowContext(), layout);

    connect(layout, &QObject::destroyed, this,
        [this, id = layout->resourceId()]()
        {
            tabs.remove(id);
            emit q->tabsChanged();
        });

    connect(tab.get(), &Tab::itemAdded, q,
        [this, tabId = tab->id()](const QJsonObject& item)
        {
            if (current->id() == tabId)
                emit q->currentTabItemAdded(item);
        });
    connect(tab.get(), &Tab::itemRemoved, q,
        [this, tabId = tab->id()](const nx::Uuid& itemId)
        {
            if (current->id() == tabId)
                emit q->currentTabItemRemoved(itemId);
        });
    connect(tab.get(), &Tab::itemChanged, q,
        [this, tabId = tab->id()](const QJsonObject& item)
        {
            if (current->id() == tabId)
                emit q->currentTabItemChanged(item);
        });

    tabs.insert(layout->resourceId(), tab);
    return tab.get();
}

Error Tabs::Private::setCurrent(Tab* tab)
{
    if (!tab)
        return Error::invalidArguments();

    workbench()->setCurrentLayout(tab->layout());
    return Error::success();
}

Tab* Tabs::Private::add(const QString& name)
{
    if (name.isEmpty())
        return nullptr;

    const QList<std::shared_ptr<Tab>> tabList = tabs.values();
    const bool exists = std::any_of(tabList.begin(), tabList.end(),
        [name](std::shared_ptr<Tab> tab) { return tab->name() == name; });

    if (exists)
        return nullptr;

    const auto layout = LayoutResourcePtr(new LayoutResource());
    layout->setIdUnsafe(nx::Uuid::createUuid());
    layout->addFlags(Qn::local);
    layout->setName(name);

    if (const auto user = system()->user())
        layout->setParentId(user->getId());

    system()->resourcePool()->addResource(layout);
    const auto workbenchLayout = workbench()->addLayout(layout);
    return addIfNeeded(workbenchLayout);
}

Error Tabs::Private::remove(Tab* tab)
{
    if (!tab)
        return Error::invalidArguments();

    // It is possible to remove a tab that contains the current web page.
    executeLater(
        [menu = menu(), layout = tab->layout()]()
        {
            menu->trigger(menu::CloseLayoutAction, QnWorkbenchLayoutList{layout});
        }, menu());

    return Error::success();
}

Tabs::Tabs(WindowContext* context, QObject* parent):
    QObject(parent),
    d(new Private(this, context))
{
}

Tabs::~Tabs()
{
}

QList<Tab*> Tabs::tabs() const
{
    QList<Tab*> result;
    for (auto [_, tab]: d->tabs.asKeyValueRange())
        result.append(tab.get());

    return result;
}

Tab* Tabs::current() const
{
    return d->current.get();
}

QJsonObject Tabs::setCurrent(Tab* tab)
{
    return detail::toJsonObject(d->setCurrent(tab));
}

QJsonObject Tabs::setCurrent(const QString& id)
{
    Tab* tab = d->tabs.value(nx::Uuid{id}).get();
    return detail::toJsonObject(d->setCurrent(tab));
}

Tab* Tabs::add(const QString& name)
{
    return d->add(name);
}

QJsonObject Tabs::remove(Tab* tab)
{
    return detail::toJsonObject(d->remove(tab));
}

QJsonObject Tabs::remove(const QString& id)
{
    Tab* tab = d->tabs.value(nx::Uuid{id}).get();
    return detail::toJsonObject(d->remove(tab));
}

} // namespace nx::vms::client::desktop::jsapi
