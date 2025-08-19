// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <api/server_rest_connection_fwd.h>
#include <nx/network/rest/params.h>

namespace nx::vms::client::core {

class OrderedRequestsHelper: public QObject
{
    using base_type = QObject;

public:
    OrderedRequestsHelper(QObject* parent = nullptr);
    ~OrderedRequestsHelper();

    bool getJsonResult(
        const rest::ServerConnectionWeakPtr& connection,
        const QString& action,
        const nx::network::rest::Params& params,
        rest::JsonResultCallback&& callback,
        QThread* thread);

    bool postJsonResult(
        const rest::ServerConnectionWeakPtr& connection,
        const QString& action,
        const nx::network::rest::Params& params,
        rest::JsonResultCallback&& callback,
        QThread* thread);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::core
