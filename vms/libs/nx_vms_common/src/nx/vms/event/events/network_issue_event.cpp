// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_issue_event.h"

#include <nx/fusion/model_functions.h>

#include <core/resource/resource.h>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace event {

static const QChar kMessageSeparator(':');

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkIssueEvent::MulticastAddressConflictParameters, (json),
    MulticastAddressConflictParameters_Fields, (brief, true))

NetworkIssueEvent::NetworkIssueEvent(
    const QnResourcePtr& resource,
    std::chrono::microseconds timestamp,
    EventReason reasonCode,
    const nx::vms::rules::NetworkIssueInfo& info)
    :
    base_type(
        EventType::networkIssueEvent,
        resource,
        timestamp.count(),
        reasonCode,
        encodeInfo(reasonCode, info))
{
}

int NetworkIssueEvent::decodeTimeoutMsecs(const QString& encoded, const int defaultValue)
{
    bool ok;
    int result = encoded.toInt(&ok);
    if (!ok)
        return defaultValue;
    return result;
}

QString NetworkIssueEvent::encodeTimeoutMsecs(int msecs)
{
    return QString::number(msecs);
}

bool NetworkIssueEvent::decodePrimaryStream(const QString& encoded, const bool defaultValue)
{
    bool ok;
    bool result = encoded.toInt(&ok);
    if (!ok)
        return defaultValue;
    return result;
}

QString NetworkIssueEvent::encodePrimaryStream(bool isPrimary)
{
    return QString::number(isPrimary);
}

QString NetworkIssueEvent::encodePrimaryStream(bool isPrimary, const QString& message)
{
    return encodePrimaryStream(isPrimary) + kMessageSeparator + message;
}

bool NetworkIssueEvent::decodePrimaryStream(
    const QString& encoded, const bool defaultValue, QString* outMessage)
{
    outMessage->clear();
    const auto delimiterPos = encoded.indexOf(kMessageSeparator);
    if (delimiterPos == -1)
        return decodePrimaryStream(encoded, defaultValue);

    *outMessage = encoded.mid(delimiterPos + 1);
    return decodePrimaryStream(encoded.left(delimiterPos), defaultValue);
}

QString NetworkIssueEvent::encodeInfo(
    EventReason reason,
    const nx::vms::rules::NetworkIssueInfo& info)
{
    using namespace std::chrono;

    switch (reason)
    {
        case EventReason::networkNoFrame:
            return encodeTimeoutMsecs(duration_cast<milliseconds>(info.timeout).count());
        case EventReason::networkRtpStreamError:
        case EventReason::networkConnectionClosed:
            return encodePrimaryStream(info.stream == nx::vms::api::StreamIndex::primary, info.message);
        case EventReason::networkMulticastAddressConflict:
            return QJson::serialized(MulticastAddressConflictParameters{
                .address = info.address,
                .deviceName = info.deviceName,
                .stream = info.stream});
        case EventReason::networkMulticastAddressIsInvalid:
            return QJson::serialized(info.address);
        default:
            return QString();
    };
}

} // namespace event
} // namespace vms
} // namespace nx
