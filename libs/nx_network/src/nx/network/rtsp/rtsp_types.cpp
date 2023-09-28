// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtsp_types.h"

#include <QtCore/QTime>

#include <nx/utils/log/log_main.h>
#include <nx/utils/std_string_utils.h>
#include <nx/utils/datetime.h>

namespace nx::network::rtsp {

namespace {

QByteArray toByteArray(std::string_view string)
{
    return QByteArray::fromRawData(string.data(), string.size());
}

std::string serializeSubseconds(int64_t timeUs)
{
    auto subseconds = QByteArray::number(timeUs % 1'000'000 / 1'000'000.0, 'f').mid(1);

    while (subseconds.endsWith('0'))
        subseconds.chop(1);

    if (subseconds == ".")
        subseconds = "";

    return subseconds.toStdString();
}

} // namespace

std::string toString(int statusCode)
{
    switch (statusCode)
    {
        case StatusCode::lowOnStorageSpace: return "Low on storage space";
        case StatusCode::parameterNotUnderstood: return "Parameter not understood";
        case StatusCode::conferenceNotFound: return "Conference not found";
        case StatusCode::notEnoughBandwidth: return "Not enough bandwidth";
        case StatusCode::sessionNotFound: return "Session not found";
        case StatusCode::methodNotValidInThisState: return "Method not valid in this state";
        case StatusCode::headerFieldNotValidForResource: return "Header field not valid for resource";
        case StatusCode::invalidRange: return "Invalid range";
        case StatusCode::parameterIsReadOnly: return "Parameter is read-only";
        case StatusCode::aggregateOperationNotAllowed: return "Aggregate operation not allowed";
        case StatusCode::onlyAggregateOperationAllowed: return "Only aggregation operation allowed";
        case StatusCode::unsupportedTransport: return "Unsupported transport";
        case StatusCode::destinationUnreachable: return "Destination unreachable";
        case StatusCode::keyManagementFailure: return "Key management failure";
        case StatusCode::rtspVersionNotSupported: return "RTSP version not supported";
        case StatusCode::optionNotSupported: return "Option not supported";
        default: return nx::network::http::StatusCode::toString(statusCode);
    }
}

std::string urlScheme(bool isSecure)
{
    return isSecure ? kSecureUrlSchemeName : kUrlSchemeName;
}

bool isUrlScheme(const std::string_view& scheme)
{
    return scheme == kUrlSchemeName || scheme == kSecureUrlSchemeName;
}

namespace header {

const std::string Range::NAME = "Range";

bool Range::parse(std::string_view serialized)
{
    serialized = serialized.substr(0, serialized.find(";"));

    const auto equalsPos = serialized.find("=");
    if (equalsPos == std::string_view::npos)
        return false;

    const auto dashPos = serialized.find("-", equalsPos);
    if (dashPos == std::string_view::npos)
        return false;

    return parseType(serialized.substr(0, equalsPos), serialized.substr(equalsPos + 1)) &&
        parseTime(&startUs, serialized.substr(equalsPos + 1, dashPos - equalsPos - 1)) &&
        parseTime(&endUs, serialized.substr(dashPos + 1));
}

std::string Range::serialize() const
{
    return NX_FMT("%1=%2-%3",
        serializeType(),
        serializeTime(startUs),
        serializeTime(endUs)
    ).toStdString();
}

bool Range::parseType(std::string_view serialized, std::string_view serializedValue)
{
    nx::utils::trim(&serialized);

    if (serialized == "npt")
    {
        type = Type::npt;
        return true;
    }

    if (serialized == "clock")
    {
        type = (serializedValue.find('T') != std::string_view::npos)
            ? Type::clock
            : Type::nxClock;
        return true;
    }

    NX_DEBUG(this, "Failed to parse RTSP range type: %1", serialized);
    return false;
}

bool Range::parseTime(std::optional<int64_t>* timeUs, std::string_view serialized)
{
    nx::utils::trim(&serialized);

    if (serialized.empty())
    {
        *timeUs = std::nullopt;
        return true;
    }

    switch (type)
    {
        case Type::npt:
            return parseNptTime(&timeUs->emplace(), serialized);
        case Type::clock:
            return parseClockTime(&timeUs->emplace(), serialized);
        case Type::nxClock:
            return parseNxClockTime(&timeUs->emplace(), serialized);
    }

    NX_ASSERT(false);
    return false;
}

bool Range::parseNptTime(int64_t* timeUs, std::string_view serialized)
{
    if (serialized == "now")
    {
        *timeUs = DATETIME_NOW;
        return true;
    }

    int64_t hours = 0;
    int64_t minutes = 0;
    double seconds = 0;

    if (const auto [parts, partCount] = nx::utils::split_n<3>(serialized, ':');
        partCount == 1 || partCount == 3)
    {
        bool ok = false;

        if (partCount == 3)
        {
            hours = toByteArray(parts[0]).toInt(&ok);
            if (!ok)
                return false;

            minutes = toByteArray(parts[1]).toInt(&ok);
            if (!ok)
                return false;
        }

        seconds = toByteArray(parts[partCount - 1]).toDouble(&ok);
        if (!ok)
            return false;
    }
    else
    {
        return false;
    }

    *timeUs = (hours * 60 + minutes) * 60 * 1'000'000 + std::llround(seconds * 1'000'000);

    return true;
}

bool Range::parseClockTime(int64_t* timeUs, std::string_view serialized)
{
    if (serialized.back() == 'Z')
        serialized.remove_suffix(1);
    else
        return false;

    const auto dotPos = std::min(serialized.find("."), serialized.size());
    if (dotPos < 2u)
        return false;

    const auto serializedSeconds = serialized.substr(dotPos - 2);
    serialized = serialized.substr(0, dotPos - 2);

    auto dateTime = QDateTime::fromString(
        QString::fromLatin1(serialized.data(), serialized.size()),
        "yyyyMMddTHHmm");
    dateTime.setTimeSpec(Qt::UTC);
    if (!dateTime.isValid())
        return false;

    bool ok = false;
    const auto seconds = toByteArray(serializedSeconds).toDouble(&ok);
    if (!ok)
        return false;

    *timeUs = dateTime.toMSecsSinceEpoch() * 1000 + std::llround(seconds * 1'000'000);

    return true;
}

bool Range::parseNxClockTime(int64_t* timeUs, std::string_view serialized)
{
    if (serialized == "now")
    {
        *timeUs = DATETIME_NOW;
        return true;
    }

    bool ok = false;
    int64_t us = toByteArray(serialized).toLongLong(&ok);
    if (!ok)
        return false;

    if (us < 1'000'000)
        us *= 1'000'000;

    *timeUs = us;

    return true;
}

std::string Range::serializeType() const
{
    switch (type)
    {
        case Type::npt:
            return "npt";
        case Type::clock:
        case Type::nxClock:
            return "clock";
    }

    NX_ASSERT(false);
    return "";
}

std::string Range::serializeTime(const std::optional<int64_t>& timeUs) const
{
    if (!timeUs)
        return "";

    switch (type)
    {
        case Type::npt:
            return serializeNptTime(*timeUs);
        case Type::clock:
            return serializeClockTime(*timeUs);
        case Type::nxClock:
            return serializeNxClockTime(*timeUs);
    }

    NX_ASSERT(false);
    return "";
}

std::string Range::serializeNptTime(int64_t timeUs) const
{
    if (timeUs == DATETIME_NOW)
        return "now";

    return QString::number((qlonglong) timeUs / 1'000'000).toStdString() +
           serializeSubseconds(timeUs);
}

std::string Range::serializeClockTime(int64_t timeUs) const
{
    if (timeUs == DATETIME_NOW)
        timeUs = nx::utils::millisSinceEpoch().count();

    const auto dateTime = QDateTime::fromSecsSinceEpoch(timeUs / 1'000'000, Qt::UTC);
    return dateTime.toString("yyyyMMddTHHmmss").toStdString() + serializeSubseconds(timeUs) + "Z";
}

std::string Range::serializeNxClockTime(int64_t timeUs) const
{
    if (timeUs == DATETIME_NOW)
        return "now";

    return QByteArray::number((qlonglong) timeUs).toStdString();
}

} // namespace header

bool RtspResponse::parse(const ConstBufferRefType &data)
{
    return nx::network::http::parseRequestOrResponse(
        data,
        (nx::network::http::Response*) this,
        &nx::network::http::Response::statusLine,
        true);
}

} // namespace nx::network::rtsp
