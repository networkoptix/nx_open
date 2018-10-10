#pragma once

/**@file
 * Utils for testing Rest API from the client side.
 */

#include <gtest/gtest.h>

#include <QJsonDocument>

#include <nx/utils/move_only_func.h>
#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>

#include <test_support/mediaserver_launcher.h>

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
// Implementation

namespace api_requests_detail {

std::unique_ptr<nx::network::http::HttpClient> createHttpClient(const QString &authName = "admin",
    const QString &authPassword = "admin");

nx::utils::Url createUrl(const MediaServerLauncher* const launcher, const QString& urlStr);

void doExecutePost(
    const MediaServerLauncher* const launcher,
    const QString& urlStr,
    const QByteArray& request,
    PreprocessRequestFunc preprocessRequestFunc,
    int httpStatus,
    const QString& authName,
    const QString& authPassword,
    QByteArray* responseBody);

/**
 * @param urlStr Part of the URL after the origin - staring with a slash, path and query.
 */
template<class RequestData>
void executePost(
    const MediaServerLauncher* const launcher,
    const QString& urlStr,
    const RequestData& requestData,
    PreprocessRequestFunc preprocessRequestFunc = nullptr,
    int httpStatus = nx::network::http::StatusCode::ok,
    const QString& authName = "admin",
    const QString& authPassword = "admin",
    QByteArray* responseBody = nullptr)
{
    QByteArray request;
    if constexpr (std::is_same<QByteArray, RequestData>::value)
        request = requestData;
    else
        request = QJson::serialized(requestData);

    ASSERT_NO_FATAL_FAILURE(doExecutePost(
        launcher, urlStr, request, std::move(preprocessRequestFunc), httpStatus, authName,
        authPassword, responseBody));
}

void doExecuteGet(
    const MediaServerLauncher* const launcher,
    const QString& urlStr,
    nx::network::http::BufferType* outResponse,
    int httpStatus,
    const QString& authName,
    const QString& authPassword);

void doExecuteGet(
    const nx::utils::Url& url,
    nx::network::http::BufferType* outResponse,
    int httpStatus,
    const QString& authName = "admin",
    const QString& authPassword = "admin");

/**
 * @param urlStr Part of the URL after the origin - staring with a slash, path and query.
 */
template<class ResponseData>
void executeGet(
    const MediaServerLauncher* const launcher,
    const QString& urlStr,
    ResponseData* responseData = nullptr,
    int httpStatus = nx::network::http::StatusCode::ok,
    const QString& authName = "admin",
    const QString& authPassword = "admin")
{
    nx::network::http::BufferType response;
    ASSERT_NO_FATAL_FAILURE(doExecuteGet(launcher, urlStr, &response, httpStatus, authName,
        authPassword));
    if (responseData)
        ASSERT_TRUE(QJson::deserialize(response, responseData));
}

void executeGet(
    const MediaServerLauncher* const launcher,
    const QString& urlStr,
    QJsonDocument* responseData,
    int httpStatus = nx::network::http::StatusCode::ok,
    const QString& authName = "admin",
    const QString& authPassword = "admin");

template<class ResponseData>
void executeGet(
    const nx::utils::Url& url,
    ResponseData* responseData = nullptr,
    int httpStatus = nx::network::http::StatusCode::ok)
{
    nx::network::http::BufferType response;
    ASSERT_NO_FATAL_FAILURE(doExecuteGet(url, &response, httpStatus));
    if (responseData)
        ASSERT_TRUE(QJson::deserialize(response, responseData));
}

void executeGet(
    const nx::utils::Url& url,
    QJsonDocument* responseData,
    int httpStatus = nx::network::http::StatusCode::ok);

} // namespace api_requests_detail

} // namespace test
} // namespace nx
