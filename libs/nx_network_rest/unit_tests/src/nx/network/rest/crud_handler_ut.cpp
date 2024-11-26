// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/rest/crud_handler.h>
#include <nx/network/rest/open_api_schema.h>
#include <nx/vms/api/data/bookmark_models.h>
#include <nx/vms/api/data/rest_api_versions.h>

namespace nx::network::rest::test {

using namespace nx::vms::api;

class BookmarkHandler: public CrudHandler<BookmarkHandler>
{
public:
    using base_type = CrudHandler<BookmarkHandler>;

    Response executePatch(const nx::network::rest::Request& request)
    {
        const bool useReflect = this->useReflect(request);
        nx::network::rest::Request::ParseInfo parseInfo;
        auto data{request.parseContentOrThrow<BookmarkWithRuleV3>(&parseInfo)};
        auto eventRuleId{data.eventRuleId};
        mergeModel(
            request, (BookmarkWithRuleV3::Bookmark*) &data, &m_bookmark, std::move(parseInfo));
        if (useReflect)
            std::swap((BookmarkWithRuleV3::Bookmark&) data, m_bookmark);

        if (auto shareValue = request.param("share"); shareValue && !shareValue->isEmpty())
        {
            auto share = QJson::deserialized<BookmarkSharingSettings>(shareValue->toUtf8());
            if (!share.password.isEmpty())
            {
                std::get<BookmarkSharingSettings>(*data.share).password =
                    BookmarkProtection::getDigest(
                        data.bookmarkId(), data.serverId(), share.password);
            }
        }
        EXPECT_EQ(data.eventRuleId, eventRuleId);
        m_bookmark = data;
        return base_type::response(data, request);
    }

public:
    BookmarkWithRuleV3::Bookmark m_bookmark;
};

static const QString kJsonTemplateV3 = R"json({
    "creationTimeMs": 0,
    "description": "",
    "deviceId": "{00000000-0000-0000-0000-000000000000}",
    "durationMs": 0,
    "id": "",
    "name": "",
    "share": {
        "expirationTimeMs": 1,
        "password": "8c1a3fc80f3607db0c6dbf47ff96e60f7cc26ad385d178c5c79be094531c979f"
    },
    "startTimeMs": 0,
    "tags": []
})json";

static const QString kJsonTemplateV4 = R"json({
    "deviceId": "{00000000-0000-0000-0000-000000000000}",
    "name": "",
    "description": "",
    "startTimeMs": 0,
    "durationMs": 0,
    "tags": [],
    "creationTimeMs": 0,
    "id": "",
    "share": {
        "expirationTimeMs": 1,
        "password": "8c1a3fc80f3607db0c6dbf47ff96e60f7cc26ad385d178c5c79be094531c979f"
    }
})json";

static const QString& jsonTemplate(std::string_view version)
{
    return version == *kRestApiV3 ? kJsonTemplateV3 : kJsonTemplateV4;
}

class CrudHandlerTest: public ::testing::TestWithParam<std::string_view>{};

TEST_P(CrudHandlerTest, BookmarkPatch)
{
    BookmarkHandler handler;
    handler.setSchemas(std::make_shared<json::OpenApiSchemas>(json::OpenApiSchemas{{
        json::OpenApiSchema::load(":/openapi_v4.json"),
        json::OpenApiSchema::load(":/openapi_v3.json")}}));
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
            Request result{&request, Content{nx::network::http::header::ContentType::kJson, body}};
            if (GetParam() == *kRestApiV3)
            {
                result.setDecodedPath("/rest/v3/bookmarks");
                result.setApiVersion(3);
            }
            else
            {
                result.setDecodedPath("/rest/v4/bookmarks");
                result.setApiVersion(4);
            }
            return result;
        };

    NX_INFO(this, "default");
    response = handler.executePatch(request(R"json({})json"));
    auto json = jsonTemplate(GetParam());
    json.replace("    \"share\": {\n", "");
    json.replace("        \"expirationTimeMs\": 1,\n", "");
    json.replace(
        "        \"password\": \"8c1a3fc80f3607db0c6dbf47ff96e60f7cc26ad385d178c5c79be094531c979f\"\n",
        "");
    if (GetParam() == *kRestApiV3)
    {
        json.replace("    },\n", "");
    }
    else
    {
        json.replace("    \"id\": \"\",\n", "    \"id\": \"\"\n");
        json.replace("    }\n", "");
    }
    ASSERT_EQ(
        nx::utils::formatJsonString(response.content->body).toStdString(), json.toStdString());

    NX_INFO(this, "expirationTimeMs and password");
    response = handler.executePatch(request(R"json(
{
    "share": {
        "expirationTimeMs": 1,
        "password": "password1"
    }
})json"));
    json = jsonTemplate(GetParam());
    ASSERT_EQ(
        nx::utils::formatJsonString(response.content->body).toStdString(), json.toStdString());

    NX_INFO(this, "expirationTimeMs");
    response = handler.executePatch(request(R"json({"share": {"expirationTimeMs": 2}})json"));
    json.replace("        \"expirationTimeMs\": 1,\n", "        \"expirationTimeMs\": 2,\n");
    ASSERT_EQ(
        nx::utils::formatJsonString(response.content->body).toStdString(), json.toStdString());

    NX_INFO(this, "password");
    response = handler.executePatch(request(R"json({"share": {"password": "password2"}})json"));
    json.replace(
        "        \"password\": \"8c1a3fc80f3607db0c6dbf47ff96e60f7cc26ad385d178c5c79be094531c979f\"\n",
        "        \"password\": \"913a9615ba626dec634e88007bb8ecebdcc399d8b9eabfbe5aad77b905431668\"\n");
    ASSERT_EQ(
        nx::utils::formatJsonString(response.content->body).toStdString(), json.toStdString());

    NX_INFO(this, "null");
    response = handler.executePatch(request(R"json({"share": null})json"));
    json.replace("        \"expirationTimeMs\": 2,\n", "");
    json.replace(
        "        \"password\": \"913a9615ba626dec634e88007bb8ecebdcc399d8b9eabfbe5aad77b905431668\"\n",
        "");
    if (GetParam() == *kRestApiV3)
    {
        json.replace("    \"share\": {\n", "    \"share\": null,\n");
        json.replace("    },\n", "");
    }
    else
    {
        json.replace("    \"share\": {\n", "    \"share\": null\n");
        json.replace("    }\n", "");
    }
    ASSERT_EQ(
        nx::utils::formatJsonString(response.content->body).toStdString(), json.toStdString());

    NX_INFO(this, "eventRuleId");
    response = handler.executePatch(
        request(R"json({"eventRuleId": "{3771fd64-9b41-4216-8800-9610d40b9c16}"})json"));
    ASSERT_TRUE(response.content->body.contains(
        R"json("eventRuleId":"{3771fd64-9b41-4216-8800-9610d40b9c16}")json"));
}

INSTANTIATE_TEST_SUITE_P(Rest, CrudHandlerTest, ::testing::ValuesIn(kRestApiV3, kRestApiEnd),
    [](auto info) { return std::string(info.param); });

} // namespace nx::network::rest::test
