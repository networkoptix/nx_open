// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tab.h"

#include <QtCore/QJsonDocument>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

#include "detail/tab_api_backend.h"
#include "detail/tab_structures.h"
#include "ui/workbench/workbench_layout.h"

#include "types.h"

namespace nx::vms::client::desktop::jsapi {

Tab::Tab(
    WindowContext* context,
    QnWorkbenchLayout* layout,
    QObject* parent)
    :
    base_type(parent),
    d(new detail::TabApiBackend(context, layout))
{
    registerTypes();

    connect(d.get(), &detail::TabApiBackend::itemAdded, this, &Tab::itemAdded);
    connect(d.get(), &detail::TabApiBackend::itemRemoved, this, &Tab::itemRemoved);
    connect(d.get(), &detail::TabApiBackend::itemChanged, this, &Tab::itemChanged);
}

Tab::~Tab()
{
}

State Tab::state() const
{
    return d->state();
}

ItemResult Tab::item(const QUuid& itemId) const
{
    return d->item(itemId);
}

ItemResult Tab::addItem(const QString& resourceId, const ItemParams& params)
{
    return d->addItem(resourceId, params);
}

Error Tab::setItemParams(const QUuid& itemId, const ItemParams& params)
{
    return d->setItemParams(itemId, params);
}

Error Tab::removeItem(const QUuid& itemId)
{
    return d->removeItem(itemId);
}

Error Tab::syncWith(const QUuid& itemId)
{
    return d->syncWith(itemId);
}

Error Tab::stopSyncPlay()
{
    return d->stopSyncPlay();
}

Error Tab::setLayoutProperties(const LayoutProperties& properties)
{
    return d->setLayoutProperties(properties);
}

Error Tab::saveLayout()
{
    return d->saveLayout();
}

QString Tab::id() const
{
    return d->layout() ? d->layout()->resourceId().toSimpleString() : QString{};
}

QString Tab::name() const
{
    return d->layout() ? d->layout()->name() : QString{};
}

QnWorkbenchLayout* Tab::layout() const
{
    return d->layout();
}

} // namespace nx::vms::client::desktop::jsapi
