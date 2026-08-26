// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "interactive_item_watcher.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/mobile/ui/interactive_item_registry.h>
#include <nx/vms/client/mobile/window_context.h>

namespace nx::vms::client::mobile {

InteractiveItemWatcher::InteractiveItemWatcher(QObject* parent):
    QObject(parent),
    WindowContextAware(WindowContext::fromQmlContext(this))
{
}

void InteractiveItemWatcher::setName(const QString& name)
{
    if (m_name == name)
        return;

    m_name = name;
    emit nameChanged();

    if (m_ready)
        reset();
}

void InteractiveItemWatcher::registerQmlType()
{
    qmlRegisterType<InteractiveItemWatcher>(
        "nx.vms.client.mobile", 1, 0, "InteractiveItemWatcher");
}

void InteractiveItemWatcher::onContextReady()
{
    if (auto context = windowContext())
        m_registry = context->interactiveItemRegistry();

    if (!NX_ASSERT(m_registry))
        return;

    connect(m_registry,
        &InteractiveItemRegistry::itemAdded,
        this,
        [this](const QString& name, InteractiveItemAttached* item)
        {
            if (name == m_name)
                add(item);
        });

    connect(m_registry,
        &InteractiveItemRegistry::itemRemoved,
        this,
        [this](const QString& name, InteractiveItemAttached* item)
        {
            if (name == m_name)
                remove(item);
        });

    m_ready = true;

    reset();
}

void InteractiveItemWatcher::add(InteractiveItemAttached* item)
{
    auto itemUpdated = [this, item]()
    {
        if (item->available())
            emit available(item->item());
    };

    m_connections[item]
        .add(connect(item,
            &InteractiveItemAttached::interacted,
            this,
            [this, item]() { emit interacted(item->item()); }))
        .add(connect(item, &InteractiveItemAttached::availableChanged, this, itemUpdated));

    itemUpdated();
}

void InteractiveItemWatcher::remove(InteractiveItemAttached* item)
{
    m_connections.erase(item);
}

void InteractiveItemWatcher::reset()
{
    m_connections.clear();

    if (m_name.isEmpty())
        return;

    if (!NX_ASSERT(m_registry))
        return;

    for (const auto& item: m_registry->attachedItems(m_name))
        add(item);
}

} // namespace nx::vms::client::mobile
