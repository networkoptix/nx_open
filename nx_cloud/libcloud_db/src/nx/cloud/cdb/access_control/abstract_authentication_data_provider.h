#pragma once

#include <functional>

#include <nx/cloud/cdb/api/result_code.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/stree/resourcecontainer.h>

namespace nx {
namespace cdb {

class AbstractAuthenticationDataProvider
{
public:
    virtual ~AbstractAuthenticationDataProvider() = default;

    /**
     * Finds entity with username and validates its password using validateHa1Func
     * In case of success, completionHandler is called with true.
     * @param authProperties Some attributes can be added here as a authentication output.
     * @param completionHandler Can be invoked within this method.
     */
    virtual void authenticateByName(
        const nx::network::http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const nx::utils::stree::AbstractResourceReader& authSearchInputData,
        nx::utils::stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::ResultCode /*authResult*/)> completionHandler) = 0;
};

} // namespace cdb
} // namespace nx
