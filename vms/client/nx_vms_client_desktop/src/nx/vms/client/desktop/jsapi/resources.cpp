// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources.h"

#include <nx/reflect/json.h>

#include "detail/resources_api_backend.h"
#include "types.h"

namespace nx::vms::client::desktop::jsapi {

Resources::Resources(QObject* parent)
    :
    base_type(parent),
    d(new detail::ResourcesApiBackend(this))
{
    registerTypes();

    connect(d.data(), &detail::ResourcesApiBackend::added, this, &Resources::added);
    connect(d.data(), &detail::ResourcesApiBackend::removed, this, &Resources::removed);
}

Resources::~Resources()
{
}

bool Resources::hasMediaStream(const QString& resourceId) const
{
    const auto result = d->resource(resourceId);
    return result.error.code == Globals::success && detail::hasMediaStream(result.resource.type);
}

QList<Resource> Resources::resources() const
{
    return d->resources();
}

ResourceResult Resources::resource(const QString& resourceId) const
{
    return d->resource(resourceId);
}

ParameterResult Resources::parameter(const QString& resourceId, const QString& name) const
{
    return d->parameter(resourceId, name);
}

ParameterResult Resources::parameterNames(const QString& resourceId) const
{
    return d->parameterNames(resourceId);
}

Error Resources::setParameter(
    const QString& resourceId,
    const QString& name,
    const QJsonValue& value)
{
    return d->setParameter(resourceId, name, value);
}

} // namespace nx::vms::client::desktop::jsapi
