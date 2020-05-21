#include "network_issue_event.h"

#include <nx/fusion/model_functions.h>

#include <core/resource/resource.h>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace event {

static const QChar kMessageSeparator(L':');

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkIssueEvent::MulticastAddressConflictParameters, (json),
    MulticastAddressConflictParameters_Fields, (brief, true))

NetworkIssueEvent::NetworkIssueEvent(
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const QString& reasonParamsEncoded)
    :
    base_type(EventType::networkIssueEvent, resource, timeStamp, reasonCode, reasonParamsEncoded)
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
    const auto delimeterPos = encoded.indexOf(kMessageSeparator);
    if (delimeterPos == -1)
        return decodePrimaryStream(encoded, defaultValue);
    
    *outMessage = encoded.mid(delimeterPos + 1);
    return decodePrimaryStream(encoded.left(delimeterPos), defaultValue);
}

} // namespace event
} // namespace vms
} // namespace nx
