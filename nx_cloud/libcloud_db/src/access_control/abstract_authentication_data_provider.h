/**********************************************************
* Dec 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_ABSTRACT_AUTHENTICATION_DATA_PROVIDER_H
#define NX_CDB_ABSTRACT_AUTHENTICATION_DATA_PROVIDER_H

#include <functional>

#include <nx/network/http/httptypes.h>
#include <plugins/videodecoder/stree/resourcecontainer.h>


namespace nx {
namespace cdb {

class AbstractAuthenticationDataProvider
{
public:
    virtual ~AbstractAuthenticationDataProvider() {}

    //!Finds entity with \a username and validates its password using \a validateHa1Func
    /*!
        In case of success, \a completionHandler is called with \a true
        \param completionHandler Can be invoked within this method
    */
    virtual void authenticateByName(
        const nx_http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        stree::AbstractResourceWriter* const authProperties,
        std::function<void(bool)> completionHandler) = 0;
};

}   //cdb
}   //nx

#endif   //NX_CDB_ABSTRACT_AUTHENTICATION_DATA_PROVIDER_H
