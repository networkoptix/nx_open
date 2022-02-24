// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/reasoned_event.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API NetworkIssueEvent: public ReasonedEvent
{
    using base_type = ReasonedEvent;
public:
    struct MulticastAddressConflictParameters
    {
        nx::network::SocketAddress address;
        QString deviceName;
        nx::vms::api::StreamIndex stream;
    };
    #define MulticastAddressConflictParameters_Fields (address)(deviceName)(stream)

public:
    explicit NetworkIssueEvent(const QnResourcePtr& resource, qint64 timeStamp,
        EventReason reasonCode, const QString& reasonParamsEncoded);

    static int decodeTimeoutMsecs(const QString& encoded, const int defaultValue);
    static QString encodeTimeoutMsecs(int msecs);

    static bool decodePrimaryStream(const QString& encoded, const bool defaultValue);
    static QString encodePrimaryStream(bool isPrimary);
    
    static QString encodePrimaryStream(bool isPrimary, const QString& message);
    static bool decodePrimaryStream(const QString& encoded, const bool defaultValue, QString* outMessage);
};

QN_FUSION_DECLARE_FUNCTIONS(NetworkIssueEvent::MulticastAddressConflictParameters,
    (json),
    NX_VMS_COMMON_API)

} // namespace event
} // namespace vms
} // namespace nx
