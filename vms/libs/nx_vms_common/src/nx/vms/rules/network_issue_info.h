// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QMetaType>
#include <QtCore/QString>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/serialization/json_fwd.h>
#include <nx/network/socket_common.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx::vms::rules {

struct NX_VMS_COMMON_API NetworkIssueInfo
{
    nx::network::SocketAddress address;
    QString deviceName;
    nx::vms::api::StreamIndex stream = nx::vms::api::StreamIndex::undefined;
    std::chrono::microseconds timeout = std::chrono::microseconds::zero();
    QString message;

    bool isPrimaryStream() const;

    bool operator==(const NetworkIssueInfo&) const = default;
};

#define NetworkIssueInfo_Fields (address)(deviceName)(stream)(timeout)(message)
QN_FUSION_DECLARE_FUNCTIONS(NetworkIssueInfo, (json), NX_VMS_COMMON_API);

} // namespace nx::vms::rules

Q_DECLARE_METATYPE(nx::vms::rules::NetworkIssueInfo);
