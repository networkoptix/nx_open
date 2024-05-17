// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_issue_event.h"

#include <core/resource/resource.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace event {

namespace {

constexpr auto kMessageSeparator = QChar(':');

QString encodeTimeoutMsecs(int msecs)
{
    return QString::number(msecs);
}

QString encodePrimaryStream(bool isPrimary)
{
    return QString::number(isPrimary);
}

QString encodePrimaryStream(bool isPrimary, const QString& message)
{
    return encodePrimaryStream(isPrimary) + kMessageSeparator + message;
}

QString encodeInfo(
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
            return encodePrimaryStream(
                info.stream == nx::vms::api::StreamIndex::primary,
                info.message);

        case EventReason::networkMulticastAddressConflict:
            return QJson::serialized(NetworkIssueEvent::MulticastAddressConflictParameters{
                .address = info.address,
                .deviceName = info.deviceName,
                .stream = info.stream});

        case EventReason::networkMulticastAddressIsInvalid:
            return QJson::serialized(info.address);

        default:
            return QString();
    };
}

} // namespace

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

int NetworkIssueEvent::decodeTimeoutMsecs(const QString& encoded)
{
    constexpr auto kDefaultTimeout = 5000;

    bool ok;
    int result = encoded.toInt(&ok);
    if (!ok)
        return kDefaultTimeout;
    return result;
}

bool NetworkIssueEvent::decodePrimaryStream(const QString& encoded)
{
    bool ok;
    bool result = encoded.toInt(&ok);
    if (!ok)
        return true;
    return result;
}

bool NetworkIssueEvent::decodePrimaryStream(const QString& encoded, QString* outMessage)
{
    outMessage->clear();
    const auto delimiterPos = encoded.indexOf(kMessageSeparator);
    if (delimiterPos == -1)
        return decodePrimaryStream(encoded);

    *outMessage = encoded.mid(delimiterPos + 1);
    return decodePrimaryStream(encoded.left(delimiterPos));
}

nx::vms::rules::NetworkIssueInfo NetworkIssueEvent::decodeInfo(const EventParameters& params)
{
    using namespace std::chrono;

    nx::vms::rules::NetworkIssueInfo info;

    switch (params.reasonCode)
    {
        case EventReason::networkNoFrame:
            info.timeout = std::chrono::milliseconds(
                decodeTimeoutMsecs(params.description));
            break;

        case EventReason::networkRtpStreamError:
        case EventReason::networkConnectionClosed:
            info.stream = decodePrimaryStream(params.description, &info.message)
                ? nx::vms::api::StreamIndex::primary
                : nx::vms::api::StreamIndex::secondary;
            break;

        case EventReason::networkMulticastAddressConflict:
        {
            MulticastAddressConflictParameters conflict;
            QJson::deserialize(params.description, &conflict);

            info.address = conflict.address;
            info.deviceName = conflict.deviceName;
            info.stream = conflict.stream;

            break;
        }
        case EventReason::networkMulticastAddressIsInvalid:
            QJson::deserialize(params.description, &info.address);
            break;

        default:
            NX_ASSERT(false, "Unexpected reason: %1", params.reasonCode);
            break;
    };

    return info;
}

} // namespace event
} // namespace vms
} // namespace nx
