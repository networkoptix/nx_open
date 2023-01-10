// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::vms::rules::utils {

class NX_VMS_RULES_API StringHelper:
    public QObject,
    public common::SystemContextAware
{
    Q_OBJECT
public:
    StringHelper(common::SystemContext* context);

    QString timestamp(
        std::chrono::microseconds eventTimestamp,
        int eventCount,
        bool html = false) const;
    QString timestampDate(std::chrono::microseconds eventTimestamp) const;
    QString timestampTime(std::chrono::microseconds eventTimestamp) const;
    QString plugin(QnUuid pluginId) const;

    QString resource(
        QnUuid resourceId,
        Qn::ResourceInfoLevel detailLevel = Qn::ResourceInfoLevel::RI_NameOnly) const;
    QString resource(
        const QnResourcePtr& resource,
        Qn::ResourceInfoLevel detailLevel = Qn::ResourceInfoLevel::RI_NameOnly) const;
    QString resourceIp(QnUuid resourceId) const;

    /**
     * Construct url, opening webadmin view for the given camera at the given time.
     * @param id Camera id.
     * @param timestamp Timestamp.
     * @param usePublicIp If the url is formed on the server side, we can force public server ip to
     *     be used instead of the local one. Actual for server-side only.
     * @param proxyAddress Force proxy address to be used. Actual for the client side only.
     */
    QString urlForCamera(
        QnUuid id,
        std::chrono::microseconds timestamp,
        bool usePublicIp,
        const std::optional<nx::network::SocketAddress>& proxyAddress = std::nullopt) const;
};

} // namespace nx::vms::rules::utils
