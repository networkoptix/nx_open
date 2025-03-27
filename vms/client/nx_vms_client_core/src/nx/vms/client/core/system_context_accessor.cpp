// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_accessor.h"

#include <QtQml/QtQml>

#include <core/resource/resource.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

struct SystemContextAccessor::Private
{
    QnResourcePtr resource;
    QPointer<SystemContext> context;
};

//-------------------------------------------------------------------------------------------------

void SystemContextAccessor::registerQmlType()
{
    qmlRegisterType<SystemContextAccessor>("nx.vms.client.core", 1, 0, "SystemContextAccessor");
}

SystemContextAccessor::SystemContextAccessor(QObject* parent):
    base_type(parent),
    d{new Private{}}
{
}

SystemContextAccessor::~SystemContextAccessor()
{
}

SystemContextAccessor::SystemContextAccessor(const QnResource* resource,
    QObject* parent)
    :
    base_type(parent),
    d{new Private{}}
{
    setRawResource(resource);
}

SystemContextAccessor::SystemContextAccessor(const QnResourcePtr resource,
    QObject* parent)
    :
    SystemContextAccessor(resource.get(), parent)
{
}

SystemContextAccessor::SystemContextAccessor(const QModelIndex& index,
    QObject* parent)
    :
    SystemContextAccessor(index.data(RawResourceRole).value<QnResource*>(), parent)
{
}

QnResourcePtr SystemContextAccessor::resource() const
{
    return d->resource;
}

QnResource* SystemContextAccessor::rawResource() const
{
    return d->resource.get();
}

void SystemContextAccessor::setRawResource(const QnResource* value)
{
    const auto resource = value
        ? value->toSharedPointer()
        : QnResourcePtr{};

    if (resource == d->resource)
        return;

    const QPointer<SystemContext> newContext = SystemContext::fromResource(resource);
    d->resource = resource;
    emit resourceChanged();

    if (newContext == d->context)
        return;

    emit systemContextIsAboutToBeChanged();
    d->context = newContext;
    emit systemContextChanged();
}

SystemContext* SystemContextAccessor::systemContext() const
{
    return d->context;
}

QnResourcePool* SystemContextAccessor::resourcePool() const
{
    return d->context
        ? d->context->resourcePool()
        : nullptr;
}

void SystemContextAccessor::updateFromModelIndex(const QModelIndex& index)
{
    setRawResource(index.data(RawResourceRole).value<QnResource*>());
}

} // namespace nx::vms::client::core
