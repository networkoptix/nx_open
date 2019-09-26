#pragma once

#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>
#include <nx/network/rest/params.h>

namespace nx::vms::client::core {

class OrderedRequestsHelper
{
public:
    OrderedRequestsHelper();
    ~OrderedRequestsHelper();

    void getJsonResult(
        const rest::QnConnectionPtr& connection,
        const QString& action,
        const nx::network::rest::Params& params,
        rest::JsonResultCallback&& callback,
        QThread* targetThread = nullptr);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::core
