// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>
#include <string>
#include <string_view>
#include <optional>

#include <nx/network/http/http_types.h>

namespace nx::network::rtsp {

enum StatusCode
{
    lowOnStorageSpace = 250,
    parameterNotUnderstood = 451,
    conferenceNotFound = 452,
    notEnoughBandwidth = 453,
    sessionNotFound = 454,
    methodNotValidInThisState = 455,
    headerFieldNotValidForResource = 456,
    invalidRange = 457,
    parameterIsReadOnly = 458,
    aggregateOperationNotAllowed = 459,
    onlyAggregateOperationAllowed = 460,
    unsupportedTransport = 461,
    destinationUnreachable = 462,
    keyManagementFailure = 463,
    rtspVersionNotSupported = 505,
    optionNotSupported = 551,
};

using StatusCodeValue = int;

NX_NETWORK_API std::string toString(int statusCode);

namespace header {

class NX_NETWORK_API Range
{
public:
    enum class Type
    {
        npt, // start/end are offsets from the beginning of recording
        clock, // start/end are UTC timestamps 
        nxClock, // start/end are UTC timestamps in non-standard format
    };

public:
    bool parse(std::string_view string);
    std::string serialize() const;

public:
    static const std::string NAME;

    Type type = Type::nxClock;
    std::optional<int64_t> startUs = {};
    std::optional<int64_t> endUs = {};

private:
    bool parseType(std::string_view serialized, std::string_view serializedValue);
    bool parseTime(std::optional<int64_t>* timeUs, std::string_view serialized);
    bool parseNptTime(int64_t* timeUs, std::string_view serialized);
    bool parseClockTime(int64_t* timeUs, std::string_view serialized);
    bool parseNxClockTime(int64_t* timeUs, std::string_view serialized);

    std::string serializeType() const;
    std::string serializeTime(const std::optional<int64_t>& timeUs) const;
    std::string serializeNptTime(int64_t timeUs) const;
    std::string serializeClockTime(int64_t timeUs) const;
    std::string serializeNxClockTime(int64_t timeUs) const;
};

} // namespace header

static constexpr char kUrlSchemeName[] = "rtsp";
static constexpr char kSecureUrlSchemeName[] = "rtsps";

NX_NETWORK_API std::string urlScheme(bool isSecure);

NX_NETWORK_API bool isUrlScheme(const std::string_view& scheme);

// Emulating explicit isUrlScheme(const QString&).
template<typename S, typename = std::enable_if_t<std::is_same_v<S, QString>>>
bool isUrlScheme(const S& s)
{
    return isUrlScheme(s.toStdString());
}

const int DEFAULT_RTSP_PORT = 554;

static const nx::network::http::MimeProtoVersion rtsp_1_0 = { "RTSP", "1.0" };

class NX_NETWORK_API RtspResponse: public nx::network::http::Response
{
public:
    bool parse(const ConstBufferRefType& data);
};

} // namespace nx::network::rtsp
