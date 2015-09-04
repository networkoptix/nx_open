/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_GENERIC_FIXED_CDB_REQUEST_H
#define NX_CDB_CL_GENERIC_FIXED_CDB_REQUEST_H

#include <functional>

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <common/common_globals.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/serialization/lexical_functions.h>

#include "include/cdb/result_code.h"


namespace nx {
namespace cdb {
namespace cl {

//!Sends request serializing as http GET request with \a input serialized to query and expecting output as json message body
/*!
    \warning On failure to initiate async request, \a handler is invoked directly from this call
*/
template<class InputData, class OutputData>
void doRequest(
    QUrl url,
    InputData input,
    std::function<void(api::ResultCode, OutputData)> handler )
{
    QUrlQuery urlQuery;
    serializeToUrlQuery( input, &urlQuery);
    urlQuery.addQueryItem("format", QnLexical::serialized(Qn::JsonFormat));
    url.setQuery(urlQuery);
    
    auto requestDoneHandler =
        [handler](
            SystemError::ErrorCode errCode,
            int statusCode,
            nx_http::BufferType msgBody)
        {

        };
    if (!nx_http::downloadFileAsync(std::move(url), std::move(requestDoneHandler)))
        handler(api::ResultCode::networkError, OutputData());
}

}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_GENERIC_FIXED_CDB_REQUEST_H
