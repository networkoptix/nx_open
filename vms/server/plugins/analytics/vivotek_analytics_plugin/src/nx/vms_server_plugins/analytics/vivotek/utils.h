#pragma once

#include <cstdint>

#include <nx/sdk/ptr.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/url.h>

#include <QtCore/QString>

namespace nx::vms_server_plugins::analytics::vivotek {

double toDouble(const QString& string);
int toInt(const QString& string);

nx::utils::Url withoutUserInfo(nx::utils::Url url);

std::int64_t parseIsoTimestamp(const QString& isoTimestamp);

inline auto toHandler(cf::promise<cf::unit> promise)
{
    return
        [promise = std::move(promise)](auto&&...) mutable
        {
            promise.set_value(cf::unit());
        };
}

template <typename Type>
auto toHandler(cf::promise<Type> promise)
{
    return
        [promise = std::move(promise)](auto&& value, auto&&...) mutable
        {
            promise.set_value(std::forward<decltype(value)>(value));
        };
}

} // namespace nx::vms_server_plugins::analytics::vivotek
