// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>

namespace nx::vms::common { class SystemContext; }

struct NX_VMS_COMMON_API QnRoute
{
    /** Address for the connect. */
    nx::network::SocketAddress addr;

    /** Target Server Id. */
    nx::Uuid id;

    /** Proxy Server Id. */
    nx::Uuid gatewayId;

    /** Whether direct connection is not available. */
    bool reverseConnect = false;

    /** Distance in peers. */
    int distance = 0;

    bool isValid() const { return !addr.isNull(); }
    QString toString() const;
};

class NX_VMS_COMMON_API QnRouter
{
public:
    static QnRoute routeTo(const nx::Uuid& serverId, nx::vms::common::SystemContext* context);
    static QnRoute routeTo(const QnMediaServerResourcePtr& server);

private:
    QnRouter() {}; // < This class should not be constructed.
};
