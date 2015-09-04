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
#include <utils/serialization/json.h>
#include <utils/serialization/lexical_functions.h>

#include "data/types.h"


namespace nx {
namespace cdb {
namespace cl {

template<typename InputData>
void serializeToUrl(const InputData& data, QUrl* const url)
{
    QUrlQuery urlQuery;
    serializeToUrlQuery(data, &urlQuery);
    urlQuery.addQueryItem("format", QnLexical::serialized(Qn::JsonFormat));
    url->setQuery(urlQuery);
}

template<typename OutputData>
void processHttpResponse(
    std::function<void(api::ResultCode, OutputData)> handler,
    SystemError::ErrorCode errCode,
    int statusCode,
    nx_http::BufferType msgBody)
{
    if (errCode != SystemError::noError)
    {
        handler(api::ResultCode::networkError, OutputData());
        return;
    }

    if (statusCode != nx_http::StatusCode::ok)
    {
        handler(api::httpStatusCodeToResultCode(
            static_cast<nx_http::StatusCode::Value>(statusCode)),
            OutputData());
        return;
    }

    bool success = false;
    OutputData outputData = QJson::deserialized<OutputData>(msgBody, OutputData(), &success);
    if (!success)
    {
        handler(api::ResultCode::networkError, OutputData());
        return;
    }

    handler(api::ResultCode::ok, outputData);
}

//!Overload for void output data
void processHttpResponseNoOutput(
    std::function<void(api::ResultCode)> handler,
    SystemError::ErrorCode errCode,
    int statusCode,
    nx_http::BufferType /*msgBody*/)
{
    if (errCode != SystemError::noError)
    {
        handler(api::ResultCode::networkError);
        return;
    }

    if (statusCode != nx_http::StatusCode::ok)
    {
        handler(api::httpStatusCodeToResultCode(
            static_cast<nx_http::StatusCode::Value>(statusCode)));
        return;
    }

    handler(api::ResultCode::ok);
}

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
    serializeToUrl(input, &url);
    
    using namespace std::placeholders;
    if (!nx_http::downloadFileAsync(
            std::move(url),
            std::bind(&processHttpResponse<OutputData>, std::move(handler), _1, _2, _3)))
    {
        handler(api::ResultCode::networkError, OutputData());
    }
}

//!Overload for void input data
template<class OutputData>
void doRequest(
    QUrl url,
    std::function<void(api::ResultCode, OutputData)> handler)
{
    using namespace std::placeholders;
    if (!nx_http::downloadFileAsync(
        std::move(url),
        std::bind(&processHttpResponse<OutputData>, std::move(handler), _1, _2, _3)))
    {
        handler(api::ResultCode::networkError, OutputData());
    }
}

//!Overload for void output data
template<class InputData>
void doRequest(
    QUrl url,
    InputData input,
    std::function<void(api::ResultCode)> handler)
{
    serializeToUrl(input, &url);

    using namespace std::placeholders;
    if (!nx_http::downloadFileAsync(
        std::move(url),
        std::bind(&processHttpResponseNoOutput, std::move(handler), _1, _2, _3)))
    {
        handler(api::ResultCode::networkError);
    }
}

//!Overload for void input & output
void doRequest(
    QUrl url,
    std::function<void(api::ResultCode)> handler)
{
    using namespace std::placeholders;
    if (!nx_http::downloadFileAsync(
        std::move(url),
        std::bind(&processHttpResponseNoOutput, std::move(handler), _1, _2, _3)))
    {
        handler(api::ResultCode::networkError);
    }
}

}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_GENERIC_FIXED_CDB_REQUEST_H
