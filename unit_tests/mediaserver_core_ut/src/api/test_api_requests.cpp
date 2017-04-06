#include "test_api_requests.h"

#include <QtCore/QSet>
#include <QtCore/QJsonDocument>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace test {

namespace api_requests_detail {

/**
 * Convert a JSON Object from string to map. If JSON is invalid or non-object, register a test
 * failure and return empty map.
 */
static QMap<QString, QVariant> jsonStrToMap(const QByteArray& jsonStr)
{
    auto json = QJsonDocument::fromJson(jsonStr);
    if (json.isNull())
    {
        ADD_FAILURE() << "Request is not a valid JSON.";
        return {};
    }

    if (!json.isObject())
    {
        ADD_FAILURE() << "Request JSON is not a JSON Object in {}.";
        return {};
    }

    return json.toVariant().toMap();
}

static QByteArray jsonMapToStr(const QMap<QString, QVariant>& jsonMap)
{
    return QJsonDocument::fromVariant(QVariant(jsonMap)).toJson();
}

// TODO: Consider moving to nx_http::HttpClient.
nx_http::BufferType readResponseBody(nx_http::HttpClient* httpClient)
{
    nx_http::BufferType response;
    while (!httpClient->eof())
        response.append(httpClient->fetchMessageBodyBuffer());
    return response;
}

std::unique_ptr<nx_http::HttpClient> createHttpClient()
{
    auto httpClient = std::make_unique<nx_http::HttpClient>();
    httpClient->setUserName("admin");
    httpClient->setUserPassword("admin");
    return httpClient;
}

QUrl createUrl(const MediaServerLauncher* const launcher, const QString& urlStr)
{
    // NOTE: urlStr contains a URL part starting after the origin: slash, path, query, etc.
    return QUrl(launcher->apiUrl().toString() + urlStr);
}

void doExecutePost(
    const MediaServerLauncher* const launcher,
    const QString& urlStr,
    const QByteArray& request,
    PreprocessRequestFunc preprocessRequestFunc,
    int httpStatus)
{
    auto httpClient = createHttpClient();
    QUrl url = createUrl(launcher, urlStr);

    const auto& actualRequest = preprocessRequestFunc ? preprocessRequestFunc(request) : request;

    NX_LOG(lm("[TEST] POST %1").arg(urlStr), cl_logINFO);
    NX_LOG(lm("[TEST] POST_REQUEST: %2").arg(actualRequest), cl_logINFO);

    httpClient->doPost(url, "application/json", actualRequest);

    const auto response = readResponseBody(httpClient.get());
    NX_LOG(lm("[TEST] POST_RESPONSE: %1").arg(response), cl_logINFO);
    NX_LOG(lm("[TEST] POST_STATUS: %1").arg(httpClient->response()->statusLine.statusCode),
        cl_logINFO);

    ASSERT_EQ(httpStatus, httpClient->response()->statusLine.statusCode);
}

void doExecuteGet(
    const MediaServerLauncher* const launcher,
    const QString& urlStr,
    nx_http::BufferType* outResponse,
    int httpStatus)
{
    auto httpClient = createHttpClient();
    QUrl url = createUrl(launcher, urlStr);

    NX_LOG(lm("[TEST] GET %1").arg(urlStr), cl_logINFO);

    ASSERT_TRUE(httpClient->doGet(url));

    NX_CRITICAL(outResponse);
    *outResponse = readResponseBody(httpClient.get());
    NX_LOG(lm("[TEST] GET_RESPONSE: %1").arg(*outResponse), cl_logINFO);
    NX_LOG(lm("[TEST] GET_STATUS: %1").arg(httpClient->response()->statusLine.statusCode),
        cl_logINFO);

    ASSERT_EQ(httpStatus, httpClient->response()->statusLine.statusCode);
}

} using namespace api_requests_detail;

PreprocessRequestFunc keepOnlyJsonFields(const QSet<QString>& fields)
{
    return
        [fields](const QByteArray& jsonStr)
        {
            auto properties = jsonStrToMap(jsonStr);
            if (properties.isEmpty())
                return jsonStr;

            for (auto it = properties.begin(); it != properties.end();)
            {
                if (fields.contains(it.key()))
                    ++it;
                else
                    it = properties.erase(it);
            }

            return jsonMapToStr(properties);
        };
}

PreprocessRequestFunc removeJsonFields(const QSet<QString>& fields)
{
    return
        [fields](const QByteArray& jsonStr)
        {
            auto properties = jsonStrToMap(jsonStr);
            if (properties.isEmpty())
                return jsonStr;

            for (auto it = properties.begin(); it != properties.end();)
            {
                if (fields.contains(it.key()))
                    it = properties.erase(it);
                else
                    ++it;
            }

            return jsonMapToStr(properties);
        };
}

} // namespace test
} // namespace nx
