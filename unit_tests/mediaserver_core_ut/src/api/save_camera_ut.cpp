#include <QJsonDocument>
#include <QFile>
#include <QRegExp>
#include <vector>

#include <gtest/gtest.h>
#include "mediaserver_launcher.h"
#include <api/app_server_connection.h>
#include <nx/network/http/httpclient.h>
#include <nx/fusion/model_functions.h>
#include <rest/server/json_rest_result.h>

QByteArray removeEmptyJsonValues(const QByteArray& data)
{
    static QRegExp uuidRegExpr("^\\{?[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}\\}?$");

    auto json = QJsonDocument::fromJson(data);
    auto properties = json.toVariant().toMap();

    for (auto itr = properties.begin(); itr != properties.end();)
    {
        const auto value = itr.value().toString();
        if (uuidRegExpr.exactMatch(value) && QnUuid(value).isNull())
            itr = properties.erase(itr);
        else if (value.isEmpty())
            itr = properties.erase(itr);
        else
            ++itr;
    }

    return QJsonDocument::fromVariant(QVariant(properties)).toJson();
}

int makeRequest(const MediaServerLauncher& launcher, const ec2::ApiCameraData& data)
{
    nx_http::HttpClient httpClient;
    httpClient.setUserName("admin");
    httpClient.setUserPassword("admin");
    QUrl url = launcher.apiUrl();
    url.setPath("/ec2/saveCamera");

    auto request = removeEmptyJsonValues(QJson::serialized(data));
    httpClient.doPost(url, "application/json", request);
    return httpClient.response()->statusLine.statusCode;
}

TEST(SaveCamera, invalidData)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    ec2::ApiCameraData cameraData;
    cameraData.id = QnUuid::createUuid();
    cameraData.parentId = QnUuid::createUuid();
    auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
    ASSERT_FALSE(resTypePtr.isNull());
    cameraData.typeId = resTypePtr->getId();
    cameraData.vendor = "test vendor";
    cameraData.physicalId = "invalid physicalId";

    auto ec2Connection = QnAppServerConnectionFactory::getConnection2();
    auto userManager = ec2Connection->getCameraManager(Qn::kSystemAccess);

    // Id doesn't match physicalId
    ASSERT_EQ(makeRequest(launcher, cameraData), nx_http::StatusCode::forbidden);

    // Auto generated Id
    cameraData.id = QnUuid();
    ASSERT_EQ(makeRequest(launcher, cameraData), nx_http::StatusCode::ok);

    // Both field correctly defined by user
    cameraData.physicalId = "valid physicalId";
    cameraData.id = ec2::ApiCameraData::physicalIdToId(cameraData.physicalId);
    ASSERT_EQ(makeRequest(launcher, cameraData), nx_http::StatusCode::ok);

    // Merged by Id with existing object
    cameraData.physicalId.clear();
    ASSERT_EQ(makeRequest(launcher, cameraData), nx_http::StatusCode::ok);

    // Both id and physicalId are empty
    cameraData.id = QnUuid();
    ASSERT_EQ(makeRequest(launcher, cameraData), nx_http::StatusCode::forbidden);
}
