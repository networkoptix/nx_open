#include "test_api_requests.h"

#include <QtCore/QSet>

namespace nx {
namespace test {

namespace api_requests_detail {

nx_http::BufferType readResponse(nx_http::HttpClient* httpClient)
{
    nx_http::BufferType response;
    while (!httpClient->eof())
        response.append(httpClient->fetchMessageBodyBuffer());
    return response;
}

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
