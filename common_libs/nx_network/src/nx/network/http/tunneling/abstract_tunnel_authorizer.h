#pragma once

#include <nx/utils/move_only_func.h>

#include "../http_types.h"
#include "../server/abstract_http_request_handler.h"
#include "../server/http_server_connection.h"

namespace nx::network::http::tunneling {

template<typename ...ApplicationData>
class TunnelAuthorizer
{
public:
    /**
     * @param statusCode Any 2xx code is considered a success.
     */
    using CompletionHandler = nx::utils::MoveOnlyFunc<void(
        StatusCode::Value /*statusCode*/,
        ApplicationData... /*applicationData*/)>;

    virtual ~TunnelAuthorizer() = default;

    /**
     * NOTE: For the sake of simplicity, an implementation is allowed 
     * to invoke completionHandler within this call.
     */
    virtual void authorize(
        const RequestContext* requestContext,
        CompletionHandler completionHandler) = 0;
};

} // namespace nx::network::http::tunneling
