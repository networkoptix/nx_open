/**********************************************************
* Dec 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_ABSTRACT_AUTHENTICATION_DATA_PROVIDER_H
#define NX_CDB_ABSTRACT_AUTHENTICATION_DATA_PROVIDER_H

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
    virtual ~AbstractAuthenticationDataProvider() {}

    /** Finds entity with \a username and validates its password using \a validateHa1Func
        In case of success, \a completionHandler is called with \a true
        @param authProperties Some attributes can be added here as a authentication output
        @param completionHandler Can be invoked within this method
    */
    virtual void authenticateByName(
        const nx_http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const nx::utils::stree::AbstractResourceReader& authSearchInputData,
        nx::utils::stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::ResultCode /*authResult*/)> completionHandler) = 0;
};

}   //cdb
}   //nx

#endif   //NX_CDB_ABSTRACT_AUTHENTICATION_DATA_PROVIDER_H
