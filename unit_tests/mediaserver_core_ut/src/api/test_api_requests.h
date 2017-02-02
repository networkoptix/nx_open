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

namespace api_requests_detail {

// TODO: Consider moving to nx_http::HttpClient.
nx_http::BufferType readResponse(nx_http::HttpClient* httpClient);

} // namespace api_requests_detail

/**
 * Perform HTTP POST Rest API request synchronously. To be used inside ASSERT_NO_FATAL_FAILURE().
 */
template<class RequestData>
void testApiPost(
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

/**
 * Perform HTTP GET Rest API request synchronously. To be used inside ASSERT_NO_FATAL_FAILURE().
 */
template<class ResponseData>
void testApiGet(
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

} // namespace test
} // namespace nx
