#pragma once

/**@file
 * Utils for testing Rest API from the client side.
 */

#include <gtest/gtest.h>

#include <nx/utils/move_only_func.h>
#include <nx/network/http/httpclient.h>
#include <nx/fusion/model_functions.h>

#include "mediaserver_launcher.h"

namespace nx {
namespace test {

typedef utils::MoveOnlyFunc<QByteArray(const QByteArray&)> PreprocessRequestFunc;

PreprocessRequestFunc keepOnlyJsonFields(const QSet<QString>& fields);

PreprocessRequestFunc removeJsonFields(const QSet<QString>& fields);

/** Perform Rest API POST request synchronously. See args in the function definition. */
#define NX_TEST_API_POST(...) ASSERT_NO_FATAL_FAILURE(api_requests_detail::executePost(__VA_ARGS__))

/** Perform Rest API GET request synchronously. See args in the function definition. */
#define NX_TEST_API_GET(...) ASSERT_NO_FATAL_FAILURE(api_requests_detail::executeGet(__VA_ARGS__))

//-------------------------------------------------------------------------------------------------
// Private

namespace api_requests_detail {

// TODO: Consider moving to nx_http::HttpClient.
nx_http::BufferType readResponse(nx_http::HttpClient* httpClient);

void doExecutePost(const MediaServerLauncher* const launcher, const QString& urlStr,
    const QByteArray& request, PreprocessRequestFunc preprocessRequestFunc, int httpStatus);

void doExecuteGet(const MediaServerLauncher* const launcher, const QString& urlStr,
    nx_http::BufferType* outResponse, int httpStatus);

template<class RequestData>
void executePost(
    const MediaServerLauncher* const launcher,
    const QString& urlStr,
    const RequestData& requestData,
    PreprocessRequestFunc preprocessRequestFunc = nullptr,
    int httpStatus = nx_http::StatusCode::ok)
{
    const QByteArray& request = QJson::serialized(requestData);
    ASSERT_NO_FATAL_FAILURE(doExecutePost(
        launcher, urlStr, request, std::move(preprocessRequestFunc), httpStatus));
}

template<class ResponseData>
void executeGet(
    const MediaServerLauncher* const launcher,
    const QString& urlStr,
    ResponseData* responseData = nullptr,
    int httpStatus = nx_http::StatusCode::ok)
{
    nx_http::BufferType response;
    ASSERT_NO_FATAL_FAILURE(doExecuteGet(launcher, urlStr, &response, httpStatus));
    if (responseData)
        ASSERT_TRUE(QJson::deserialize(response, responseData));
}

} // namespace api_requests_detail

} // namespace test
} // namespace nx
