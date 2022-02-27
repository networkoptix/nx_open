// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_module_aware.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>

namespace nx { namespace vms { namespace discovery { class Manager; } } }

struct NX_VMS_COMMON_API QnRoute
{

    nx::network::SocketAddress addr; // address for physical connect
    QnUuid id;          // requested server ID
    QnUuid gatewayId;   // proxy server ID. May be null
    bool reverseConnect;// if target server should connect to this one
    int distance;

    QnRoute(): reverseConnect(false), distance(0) {}
    bool isValid() const { return !addr.isNull(); }
    QString toString() const;
};

class NX_VMS_COMMON_API QnRouter:
    public QObject,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
public:
    explicit QnRouter(
        QObject* parent,
        nx::vms::discovery::Manager* moduleManager);
    ~QnRouter();

    // todo: new routing functions below. We have to delete above functions
    QnRoute routeTo(const QnUuid &id);

private:
    const nx::vms::discovery::Manager* m_moduleManager;
};
