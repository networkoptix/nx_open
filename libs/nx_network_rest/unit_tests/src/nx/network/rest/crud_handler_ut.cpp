// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/rest/crud_handler.h>
#include <nx/network/rest/open_api_schema.h>
#include <nx/vms/api/data/bookmark_models.h>

namespace nx::network::rest::test {

using namespace nx::vms::api;

class BookmarkHandler: public CrudHandler<BookmarkHandler>
{
public:
    using base_type = CrudHandler<BookmarkHandler>;

    Response executePatch(const nx::network::rest::Request& request)
    {
        std::optional<QJsonValue> incomplete;
        auto data =
            request.parseContentAllowingOmittedValuesOrThrow<BookmarkWithRuleV3>(&incomplete);
        if (incomplete)
        {
            QString error;
            if (!nx::network::rest::json::merge(&data,
                m_bookmark,
                *incomplete,
                &error,
                /*chronoSerializedAsDouble*/ true))
            {
                throw nx::network::rest::Exception::badRequest(error);
            }
        }

        if (auto shareValue = request.param("share"); shareValue && !shareValue->isEmpty())
        {
            auto share = QJson::deserialized<BookmarkSharingSettings>(shareValue->toUtf8());
            if (!share.password.isEmpty())
            {
                std::get<BookmarkSharingSettings>(*data.share).password =
                    BookmarkProtection::getDigest(data.id, share.password);
            }
        }
        m_bookmark = data;
        return base_type::response(data, request);
    }

public:
    BookmarkWithRuleV3 m_bookmark;
};

TEST(CrudHandler, BookmarkPatch)
{
    BookmarkHandler handler;
    handler.setSchemas(std::make_shared<json::OpenApiSchemas>(
        json::OpenApiSchemas{{json::OpenApiSchema::load(":/openapi.json")}}));
    Response response;
    const auto request =
        [&](const QByteArray& body)
        {
            static auto request =
                []()
                {
                    nx::network::http::Request r;
                    r.requestLine.method = nx::network::http::Method::patch;
                    return r;
                }();
            Request result{&request, kSystemSession};
            result.setDecodedPath("/rest/v3/bookmarks");
            result.content = Content{nx::network::http::header::ContentType::kJson, body};
            return result;
        };

    NX_INFO(this, "Default");
    response = handler.executePatch(request(R"json({})json"));
    ASSERT_EQ(nx::utils::formatJsonString(response.content->body).toStdString(),
        /*suppress newline*/ 1 + (const char*) R"json(
{
    "creationTimeMs": 0,
    "description": "",
    "deviceId": "{00000000-0000-0000-0000-000000000000}",
    "durationMs": 0,
    "id": "",
    "name": "",
    "startTimeMs": 0,
    "tags": []
})json");

    NX_INFO(this, "expirationTimeMs and password");
    response = handler.executePatch(request(R"json(
{
    "share": {
        "expirationTimeMs": 1,
        "password": "password1"
    }
})json"));
    ASSERT_EQ(nx::utils::formatJsonString(response.content->body).toStdString(),
        /*suppress newline*/ 1 + (const char*) R"json(
{
    "creationTimeMs": 0,
    "description": "",
    "deviceId": "{00000000-0000-0000-0000-000000000000}",
    "durationMs": 0,
    "id": "",
    "name": "",
    "share": {
        "expirationTimeMs": 1,
        "password": "0b14d501a594442a01c6859541bcb3e8164d183d32937b851835442f69d5c94e"
    },
    "startTimeMs": 0,
    "tags": []
})json");

    NX_INFO(this, "expirationTimeMs");
    response = handler.executePatch(request(R"json(
{
    "share": {
        "expirationTimeMs": 2
    }
})json"));
    ASSERT_EQ(nx::utils::formatJsonString(response.content->body).toStdString(),
        /*suppress newline*/ 1 + (const char*) R"json(
{
    "creationTimeMs": 0,
    "description": "",
    "deviceId": "{00000000-0000-0000-0000-000000000000}",
    "durationMs": 0,
    "id": "",
    "name": "",
    "share": {
        "expirationTimeMs": 2,
        "password": "0b14d501a594442a01c6859541bcb3e8164d183d32937b851835442f69d5c94e"
    },
    "startTimeMs": 0,
    "tags": []
})json");

    NX_INFO(this, "password");
    response = handler.executePatch(request(R"json(
{
    "share": {
        "password": "password2"
    }
})json"));
    ASSERT_EQ(nx::utils::formatJsonString(response.content->body).toStdString(),
        /*suppress newline*/ 1 + (const char*) R"json(
{
    "creationTimeMs": 0,
    "description": "",
    "deviceId": "{00000000-0000-0000-0000-000000000000}",
    "durationMs": 0,
    "id": "",
    "name": "",
    "share": {
        "expirationTimeMs": 2,
        "password": "6cf615d5bcaac778352a8f1f3360d23f02f34ec182e259897fd6ce485d7870d4"
    },
    "startTimeMs": 0,
    "tags": []
})json");

    NX_INFO(this, "null");
    response = handler.executePatch(request(R"json({"share": null})json"));
    ASSERT_EQ(nx::utils::formatJsonString(response.content->body).toStdString(),
        /*suppress newline*/ 1 + (const char*) R"json(
{
    "creationTimeMs": 0,
    "description": "",
    "deviceId": "{00000000-0000-0000-0000-000000000000}",
    "durationMs": 0,
    "id": "",
    "name": "",
    "share": null,
    "startTimeMs": 0,
    "tags": []
})json");
}

} // namespace nx::network::rest::test
