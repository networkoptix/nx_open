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
    std::function<bool()> isSupportedCondition,
    QObject* parent)
    :
    QObject(parent),
    d(new detail::TabApiBackend(context, layout)),
    m_isSupportedCondition(isSupportedCondition)
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
    return ensureSupported() ? d->state() : State{};
}

ItemResult Tab::item(const QUuid& itemId) const
{
    return ensureSupported() ? d->item(itemId) : ItemResult{};
}

ItemResult Tab::addItem(const QString& resourceId, const ItemParams& params)
{
    return ensureSupported() ? d->addItem(resourceId, params) : ItemResult{};
}

Error Tab::setItemParams(const QUuid& itemId, const ItemParams& params)
{
    return ensureSupported() ? d->setItemParams(itemId, params) : Error{};
}

Error Tab::setMaximizedItem(const QString& itemId)
{
    return ensureSupported() ? d->setMaximizedItem(itemId) : Error{};
}

Error Tab::removeItem(const QUuid& itemId)
{
    return ensureSupported() ? d->removeItem(itemId) : Error{};
}

Error Tab::syncWith(const QUuid& itemId)
{
    return ensureSupported() ? d->syncWith(itemId) : Error{};
}

Error Tab::stopSyncPlay()
{
    return ensureSupported() ? d->stopSyncPlay() : Error{};
}

Error Tab::setLayoutProperties(const LayoutProperties& properties)
{
    return ensureSupported() ? d->setLayoutProperties(properties) : Error{};
}

Error Tab::saveLayout()
{
    return ensureSupported() ? d->saveLayout() : Error{};
}

QString Tab::id() const
{
    // Do not check ensureSupported, because property values may be requested by qt during
    // initialization.
    return d->layout() ? d->layout()->resourceId().toSimpleString() : QString{};
}

QString Tab::name() const
{
    // Do not check ensureSupported, because property values may be requested by qt during
    // initialization.
    return d->layout() ? d->layout()->name() : QString{};
}

QnWorkbenchLayout* Tab::layout() const
{
    return d->layout();
}

bool Tab::ensureSupported() const
{
    return !m_isSupportedCondition || m_isSupportedCondition();
}

} // namespace nx::vms::client::desktop::jsapi
