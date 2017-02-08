#pragma once

/**@file
 * Utils for testing Rest API from the client side.
 */

#include <QtCore/QJsonDocument>

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/network/http/httpclient.h>
#include <nx/fusion/model_functions.h>

#include "mediaserver_launcher.h"

namespace nx {
namespace test {

typedef nx::utils::MoveOnlyFunc<QByteArray(const QByteArray&)> PreprocessRequestFunc;

PreprocessRequestFunc keepOnlyJsonFields(const QSet<QString>& fields);

PreprocessRequestFunc removeJsonFields(const QSet<QString>& fields);

/** Perform Rest API POST request synchronously. */
#define NX_TEST_API_POST(...) ASSERT_NO_FATAL_FAILURE(api_requests_detail::executePost(__VA_ARGS__))

/** Perform Rest API GET request synchronously. */
#define NX_TEST_API_GET(...) ASSERT_NO_FATAL_FAILURE(api_requests_detail::executeGet(__VA_ARGS__))

namespace api_requests_detail {

// TODO: Consider moving to nx_http::HttpClient.
nx_http::BufferType readResponse(nx_http::HttpClient* httpClient);

template<class RequestData>
void executePost(
    const MediaServerLauncher& launcher,
    const QString& urlStr,
    const RequestData& requestData,
    PreprocessRequestFunc preprocessRequestFunc = nullptr,
    int httpStatus = nx_http::StatusCode::ok)
{
    nx_http::HttpClient httpClient;
    httpClient.setUserName("admin");
    httpClient.setUserPassword("admin");
    QUrl url = launcher.apiUrl();
    url.setPath(urlStr);

    QByteArray request = QJson::serialized(requestData);
    if (preprocessRequestFunc)
        request = preprocessRequestFunc(request);

    NX_LOG(lm("[TEST] POST %1").arg(urlStr), cl_logINFO);
    NX_LOG(lm("[TEST] POST_REQUEST: %2").arg(request), cl_logINFO);

    httpClient.doPost(url, "application/json", request);

    const auto response = api_requests_detail::readResponse(&httpClient);
    NX_LOG(lm("[TEST] POST_RESPONSE: %1").arg(response), cl_logINFO);
    NX_LOG(lm("[TEST] POST_STATUS: %1").arg(httpClient.response()->statusLine.statusCode),
        cl_logINFO);

    ASSERT_EQ(httpStatus, httpClient.response()->statusLine.statusCode);
}

template<class ResponseData>
void executeGet(
    const MediaServerLauncher& launcher,
    const QString& urlStr,
    ResponseData* responseData = nullptr,
    int httpStatus = nx_http::StatusCode::ok)
{
    nx_http::HttpClient httpClient;
    httpClient.setUserName("admin");
    httpClient.setUserPassword("admin");
    QUrl url = launcher.apiUrl();
    url.setPath(urlStr);

    NX_LOG(lm("[TEST] GET %1").arg(urlStr), cl_logINFO);

    ASSERT_TRUE(httpClient.doGet(url));

    const auto response = api_requests_detail::readResponse(&httpClient);
    NX_LOG(lm("[TEST] GET_RESPONSE: %1").arg(response), cl_logINFO);
    NX_LOG(lm("[TEST] GET_STATUS: %1").arg(httpClient.response()->statusLine.statusCode),
        cl_logINFO);

    ASSERT_EQ(httpStatus, httpClient.response()->statusLine.statusCode);

    if (responseData)
        ASSERT_TRUE(QJson::deserialize(response, responseData));
}

} // namespace api_requests_detail

} // namespace test
} // namespace nx
