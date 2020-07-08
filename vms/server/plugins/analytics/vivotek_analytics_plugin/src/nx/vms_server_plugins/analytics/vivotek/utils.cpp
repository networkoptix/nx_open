#include "utils.h"

#include <QtCore/QDateTime>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::utils;

Url withoutUserInfo(Url url)
{
    url.setUserInfo("");
    return url;
}

std::int64_t parseIsoTimestamp(const QString& isoTimestamp)
{
    const auto dateTime = QDateTime::fromString(isoTimestamp, Qt::ISODateWithMs);
    if (!dateTime.isValid())
        throw Exception("Failed to parse iso timestamp: %1", isoTimestamp);
    return dateTime.toMSecsSinceEpoch() * 1000; // to microseconds
}

} // namespace nx::vms_server_plugins::analytics::vivotek
