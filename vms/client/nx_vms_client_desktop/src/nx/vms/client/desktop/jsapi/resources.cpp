// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources.h"

#include <nx/reflect/json.h>

#include "detail/helpers.h"
#include "detail/resources_api_backend.h"

namespace nx::vms::client::desktop::jsapi {

Resources::Resources(QObject* parent)
    :
    base_type(parent),
    d(new detail::ResourcesApiBackend(this))
{
    connect(d.data(), &detail::ResourcesApiBackend::added, this,
        [this](const detail::Resource& resource)
        {
            emit added(detail::toJsonObject(resource));
        });
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

QJsonArray Resources::resources() const
{
    return detail::toJsonArray(d->resources());
}

QJsonObject Resources::resource(const QString& resourceId) const
{
    return detail::toJsonObject(d->resource(resourceId));
}

} // namespace nx::vms::client::desktop::jsapi
