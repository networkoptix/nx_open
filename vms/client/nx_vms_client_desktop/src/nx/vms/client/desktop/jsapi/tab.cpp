// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tab.h"

#include <QtCore/QJsonDocument>

#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

#include "detail/helpers.h"
#include "detail/tab_api_backend.h"
#include "detail/tab_structures.h"

namespace nx::vms::client::desktop::jsapi {

Tab::Tab(
    QnWorkbenchContext* context,
    QObject* parent)
    :
    base_type(parent),
    d(new detail::TabApiBackend(context, context->workbench()->currentLayout()))
{
    using namespace detail;

    connect(d.get(), &TabApiBackend::itemAdded, this,
        [this](const Item& item)
        {
            emit itemAdded(toJsonObject(item));
        });

    connect(d.get(), &detail::TabApiBackend::itemRemoved, this, &Tab::itemRemoved);

    connect(d.get(), &detail::TabApiBackend::itemChanged, this,
        [this](const Item& item)
        {
            emit itemChanged(toJsonObject(item));
        });
}

Tab::~Tab()
{
}

QJsonObject Tab::state() const
{
    return toJsonObject(d->state());
}

QJsonObject Tab::item(const QUuid& itemId) const
{
    return toJsonObject(d->item(itemId));
}

QJsonObject Tab::addItem(const QString& resourceId, const QJsonObject& params)
{
    detail::ItemParams itemParams;
    return detail::toJsonObject(fromJsonObject(params, itemParams)
        ? d->addItem(resourceId, itemParams)
        : detail::ItemResult{Error::invalidArguments(), {}});
}

QJsonObject Tab::setItemParams(const QUuid& itemId, const QJsonObject& params)
{
    detail::ItemParams itemParams;
    return detail::toJsonObject(fromJsonObject(params, itemParams)
        ? d->setItemParams(itemId, itemParams)
        : Error::invalidArguments());
}

QJsonObject Tab::removeItem(const QUuid& itemId)
{
    return detail::toJsonObject(d->removeItem(itemId));
}

QJsonObject Tab::syncWith(const QUuid& itemId)
{
    return detail::toJsonObject(d->syncWith(itemId));
}

QJsonObject Tab::stopSyncPlay()
{
    return detail::toJsonObject(d->stopSyncPlay());
}

QJsonObject Tab::setLayoutProperties(const QJsonObject& properties)
{
    detail::LayoutProperties layoutProperties;
    return detail::toJsonObject(fromJsonObject(properties, layoutProperties)
        ? d->setLayoutProperties(layoutProperties)
        : Error::invalidArguments());
}

QJsonObject Tab::saveLayout()
{
    return detail::toJsonObject(d->saveLayout());
}

} // namespace nx::vms::client::desktop::jsapi
