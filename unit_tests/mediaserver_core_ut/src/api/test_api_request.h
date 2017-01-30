#pragma once

/**@file
 * Utils for testing Rest API from the client side.
 */

#include <QJsonDocument>

#include <nx/utils/move_only_func.h>
#include <nx/network/http/httpclient.h>
#include <nx/fusion/model_functions.h>

#include "mediaserver_launcher.h"

namespace nx {
namespace test {
namespace {

typedef nx::utils::MoveOnlyFunc<QByteArray(const QByteArray&)> PreprocessJsonFunc;

template<class RequestData>
void testApiPost(
    const MediaServerLauncher& launcher,
    const QString& urlStr,
    const RequestData& requestData,
    PreprocessJsonFunc preprocessJsonFunc = nullptr,
    int httpStatus = nx_http::StatusCode::ok)
{
    nx_http::HttpClient httpClient;
    httpClient.setUserName("admin");
    httpClient.setUserPassword("admin");
    QUrl url = launcher.apiUrl();
    url.setPath(urlStr);

    QByteArray request = QJson::serialized(requestData);
    if (preprocessJsonFunc)
        request = preprocessJsonFunc(request);

    httpClient.doPost(url, "application/json", request);
    ASSERT_EQ(httpStatus, httpClient.response()->statusLine.statusCode);
}

static PreprocessJsonFunc keepOnlyJsonFields(const QSet<QString>& fields)
{
    return
        [fields](const QByteArray& jsonStr)
        {
            auto json = QJsonDocument::fromJson(jsonStr);
            auto properties = json.toVariant().toMap();

            for (auto itr = properties.begin(); itr != properties.end();)
            {
                if (fields.contains(itr.key()))
                    ++itr;
                else
                    itr = properties.erase(itr);
            }

            return QJsonDocument::fromVariant(QVariant(properties)).toJson();
        };
}

static PreprocessJsonFunc removeJsonFields(const QSet<QString>& fields)
{
    return
        [fields](const QByteArray& jsonStr)
        {
            auto json = QJsonDocument::fromJson(jsonStr);
            auto properties = json.toVariant().toMap();

            for (auto itr = properties.begin(); itr != properties.end();)
            {
                if (fields.contains(itr.key()))
                    itr = properties.erase(itr);
                else
                    ++itr;
            }

            return QJsonDocument::fromVariant(QVariant(properties)).toJson();
        };
}

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

    ASSERT_TRUE(httpClient.doGet(url));

    ASSERT_EQ(httpStatus, httpClient.response()->statusLine.statusCode);

    if (responseData)
    {
        const auto responseBody = httpClient.fetchMessageBodyBuffer();
        ASSERT_TRUE(QJson::deserialize(responseBody, responseData));
    }
}

} // namespace
} // namespace test
} // namespace nx
