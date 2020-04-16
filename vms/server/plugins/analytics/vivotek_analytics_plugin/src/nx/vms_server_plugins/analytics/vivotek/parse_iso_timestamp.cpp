#include "parse_iso_timestamp.h"

#include <stdexcept>

#include <QtCore/QDateTime>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;

std::int64_t parseIsoTimestamp(std::string_view isoTimestamp)
{
    const auto qIsoTimestamp = QString::fromLatin1(isoTimestamp.data(), isoTimestamp.size());
    const auto dateTime = QDateTime::fromString(qIsoTimestamp, Qt::ISODateWithMs);
    if (!dateTime.isValid())
        throw std::runtime_error("Invalid iso timestamp: " + std::string(isoTimestamp));
    return dateTime.toMSecsSinceEpoch() * 1000;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
