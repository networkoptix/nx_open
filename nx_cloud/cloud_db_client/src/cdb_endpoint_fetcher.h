/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_ENDPOINT_FETCHER_H
#define NX_CDB_ENDPOINT_FETCHER_H

#include <functional>

#include <boost/optional.hpp>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include "include/cdb/result_code.h"


namespace nx {
namespace cdb {
namespace cl {


//!Retrieves url to the specified cloud module
class CloudModuleEndPointFetcher
{
public:
    //!Retrieves endpoint if unknown. If endpoint is known, then calls \a handler directly from this method
    void get(
        QString username,
        QString password,
        std::function<void(api::ResultCode, SocketAddress)> handler)
    {
        //TODO #ak if requested endpoint is known, providing it to the output
        //TODO #ak if requested url is unknown, fetching description xml

        //TODO #ak
    }

private:
    boost::optional<SocketAddress> m_endpoint;
};


}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_ENDPOINT_FETCHER_H
