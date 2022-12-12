// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/event/events/reasoned_event.h>
#include <nx/vms/rules/network_issue_info.h>

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
    NetworkIssueEvent(
        const QnResourcePtr& resource,
        std::chrono::microseconds timestamp,
        EventReason reasonCode,
        const nx::vms::rules::NetworkIssueInfo& info);

    static int decodeTimeoutMsecs(const QString& encoded);
    static bool decodePrimaryStream(const QString& encoded);
    static bool decodePrimaryStream(const QString& encoded, QString* outMessage);
    static nx::vms::rules::NetworkIssueInfo decodeInfo(const EventParameters& params);
};

QN_FUSION_DECLARE_FUNCTIONS(NetworkIssueEvent::MulticastAddressConflictParameters,
    (json),
    NX_VMS_COMMON_API)

} // namespace event
} // namespace vms
} // namespace nx
