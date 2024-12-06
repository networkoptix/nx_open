// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "openapi_schema_ut.h"

#include <memory>

#include <gtest/gtest.h>

#include <nx/network/rest/crud_handler.h>
#include <nx/network/rest/handler_pool.h>
#include <nx/network/rest/open_api_schema.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/json.h>
#include <nx/utils/log/log.h>
#include <nx/utils/reflect.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/rest_api_versions.h>

namespace nx::network::rest::json::test {

NX_REFLECTION_INSTRUMENT(NestedTestData, (id)(name))
NX_REFLECTION_INSTRUMENT(TestData, (id)(list)(name))

using namespace nx::vms::api;
using GlobalPermission = HandlerPool::GlobalPermission;

class OpenApiSchemaTest: public ::testing::TestWithParam<std::string_view>
{
public:
    virtual void SetUp() override
    {
        m_schemas = std::make_shared<OpenApiSchemas>(std::vector<std::shared_ptr<OpenApiSchema>>{
            OpenApiSchema::load(":/openapi_v4.json"), OpenApiSchema::load(":/openapi_v3.json")});
    }

    std::unique_ptr<Request> restRequest(QStringList text, QByteArray body = {})
    {
        text.front().replace("/rest/{version}/", NX_FMT("/rest/%1/", GetParam()));
        http::Request httpRequest;
        httpRequest.parse(text.join("\r\n").toUtf8());
        QString path = httpRequest.requestLine.url.path();

        m_requests.push_front(httpRequest);
        std::optional<rest::Content> content;
        if (!body.isEmpty())
            content = {http::header::ContentType::kJson, std::move(body)};
        auto request = std::make_unique<Request>(&m_requests.front(), std::move(content));

        if (path.startsWith("/rest/"))
        {
            const auto name = path.split('/')[2];
            EXPECT_TRUE(name.startsWith('v'))
                << NX_FMT("API version '%1' is not supported", name).toStdString();

            bool isOk = false;
            const auto number = name.mid(1).toInt(&isOk);
            EXPECT_TRUE(isOk && number > 0)
                << NX_FMT("API version '%1' is not supported", name).toStdString();

            request->setApiVersion((size_t) number);
        }

        return request;
    }

    template<typename Model>
    class MockCrudHandler: public CrudHandler<MockCrudHandler<Model>>
    {
    public:
        std::vector<Model> read(IdData, const Request&)
        {
            Model model;
            model.name = "\u0414\u043e\u043c";
            return {std::move(model)};
        }

        void create(Model, const Request&) {}
    };

    class MockBase64Handler: public CrudHandler<MockBase64Handler>
    {
    public:
        MockBase64Handler(): CrudHandler<MockBase64Handler>(/*idParamName*/ "param") {}
        std::vector<Base64Model> read(Base64Model filter, const Request&)
        {
            return {std::move(filter)};
        }
    };

    class MockMapHandler: public CrudHandler<MockMapHandler>
    {
    public:
        MockMapHandler(): CrudHandler<MockMapHandler>(/*idParamName*/ "fakeId") {}
        TestDataMap read(IdData, const Request&) { return m_data; }
        void update(TestDataMap data, const Request&) { m_data = std::move(data); }

    private:
        TestDataMap m_data;
    };

    template<typename Model>
    class MockHandler: public Handler
    {
    public:
        virtual Response executeGet(const Request&) override
        {
            Response response(http::StatusCode::ok);
            response.content = {
                http::header::ContentType::kJson, QJson::serialized(std::vector<Model>(1))};
            return response;
        }

        virtual Response executePost(const Request&) override
        {
            return http::StatusCode::ok;
        }

    private:
        // It is empty overridden to disable default validation against OpenAPI schema.
        virtual void validateAndAmend(Request*, nx::network::http::HttpHeaders*) override {}
    };

    void testPerformance(
        HandlerPool* pool, const QStringList& text, const QByteArray& body = {})
    {
        const int testCount = 1000;
        nx::utils::ElapsedTimer timer(nx::utils::ElapsedTimerState::started);
        for (int i = 0; i < testCount; ++i)
        {
            std::unique_ptr<Request> request = restRequest(text, body);
            auto handler = pool->findHandlerOrThrow(request.get());
            ASSERT_TRUE(handler);
            Response response = handler->executeRequestOrThrow(request.get());
            ASSERT_EQ(response.statusCode, http::StatusCode::ok);
        }
        NX_INFO(this, "Made %1 API requests in %2", testCount, timer.elapsed());
    }

    void postprocessResponse(const Request& request, QJsonValue* json)
    {
        m_schemas->postprocessResponse(
            request.method(), request.params(), request.decodedPath(), json);
    }

    void postprocessResponse(const Request& request, rapidjson::Document* json)
    {
        m_schemas->postprocessResponse(request.method(), request.decodedPath(), json);
    }

    std::shared_ptr<OpenApiSchemas> m_schemas;

private:
    std::list<http::Request> m_requests;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TestDeviceModel, (json), (general)(credentials)(attributes)(status))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Base64Model, (json), (param))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NestedTestData, (json), (id)(name))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TestData, (json), (id)(name)(list))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ArrayOrderedTestNested, (json), (name))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ArrayOrderedTestVariant, (json), (id))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ArrayOrdererTestResponse, (json), (map))
NX_REFLECTION_INSTRUMENT(Base64Model, (param))

[[maybe_unused]] static void dummyRegisterFunction()
{
    const auto reg = [](auto&&...) {};
    /**%apidoc GET /rest/v{3-}/test
     * %ingroup Test
     * %return List of test data items.
     *     %struct TestDataList
     */
    reg("rest/test/:id?");

    /**%apidoc GET /rest/v{3-}/devices
     * %ingroup Test
     * %return List of all device information objects.
     *     %struct TestDeviceModelList
     *
     * %apidoc POST /rest/v{3-}/devices
     * Create a device.
     * %ingroup Test
     * %struct TestDeviceModel
     * %param [unused] general.id
     * %return Device information object that was created.
     *     %struct TestDeviceModel
     *
     * %apidoc GET /rest/v{3-}/devices/{id}
     * Read the device specified by {id}. The device must be created before.
     * %ingroup Test
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via GET on /rest/v{3-}/devices) or MAC address (not supported for certain cameras).
     * %return Device information object.
     *     %struct TestDeviceModel
     *
     * %apidoc PUT /rest/v{3-}/devices/{id}
     * Completely replace the device specified by {id} with new data provided.
     * The device must be created before.
     * %ingroup Test
     * %struct TestDeviceModel
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via GET on /rest/v{3-}/devices) or MAC address (not supported for certain devices).
     * %return Device information object.
     *     %struct TestDeviceModel
     *
     * %apidoc PATCH /rest/v{3-}/devices/{id}
     * Patch the device specified by {id} with new data provided.
     * The device must be created before.
     * %ingroup Test
     * %struct [opt] TestDeviceModel
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via GET on /rest/v{3-}/devices) or MAC address (not supported for certain cameras).
     * %return Device information object.
     *     %struct TestDeviceModel
     *
     * %apidoc DELETE /rest/v{3-}/devices/{id}
     * Delete the device specified by {id}. The device must be created before.
     * %ingroup Test
     * %param:string id Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via GET on /rest/v{3-}/devices) or MAC address (not supported for certain cameras).
     */
    reg("rest/v{3-}/devices/:id?");

    /**%apidoc GET /rest/v{3-}/layouts
     * %ingroup Test
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_pretty,_with
     * %return List of all layout information objects.
     *     %struct LayoutDataList
     *
     * %apidoc POST /rest/v{3-}/layouts
     * Create a layout.
     * %ingroup Test
     * %struct LayoutData
     * %return Layout information object created.
     *     %struct LayoutData
     *
     * %apidoc GET /rest/v{3-}/layouts/{id}
     * Read the layout specified by {id}. The layout must be created before.
     * %ingroup Test
     * %param:string id Layout unique id or logical id.
     * %return Layout information object.
     *     %struct LayoutData
     */
    reg("rest/v{3-}/layouts/:id?");

    /**%apidoc POST /rest/v{3-}/settings
     * %ingroup Test
     * %struct KeyValueData
     * %param:any value
     */
    reg("rest/v{3-}/settings");

    /**%apidoc GET /rest/v{3-}/base64/{param}/echo
     * %ingroup Test
     * %param:base64 param
     * %return
     *     %struct Base64Model
     */
    reg("rest/v{3-}/base64/:param/echo");

    /**%apidoc GET /rest/v{3-}/map
     * %ingroup Test
     * %param[ref] _with
     * %return:{std::map<nx::Uuid, TestData>}
     *     %param[unused] *.id
     *
     **%apidoc:{std::map<nx::Uuid, TestData>} PUT /rest/v{3-}/map
     * %ingroup Test
     * %param[unused] *.id
     * %return:{std::map<nx::Uuid, TestData>}
     *     %param[unused] *.id
     */
    reg("rest/v{3-}/map");

    /**%apidoc PATCH /rest/v{3-}/devices/{deviceId}/bookmarks/{id}
     * Modifies certain fields of the particular Bookmark record stored in the Site.
     * %caption Modify Bookmark
     * %ingroup Device Media
     * %struct[opt] BookmarkV3
     *     %param[unused] creationTimeMs
     * %permissions Manage bookmarks.
     * %return:{BookmarkV3} Bookmark record.
     *     %param[unused] password
     */
    reg("rest/v{3-}/bookmarks");

    /**%apidoc GET /rest/v{4-}/analytics/objectTracks
     * Searches the Analytics DB for Objects that match the specified filter, and retrieves the
     * list of the matching Object Tracks.
     * %caption Get Object Tracks
     * %ingroup Analytics
     * %struct ObjectTrackFilter
     * %param[unused] id
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{std::vector<ObjectTrackV4>} Object Tracks.
     *
     **%apidoc GET /rest/v{4-}/analytics/objectTracks/{id}
     * Retrieves the specified Object Track.
     * %caption Get Object Track
     * %ingroup Analytics
     * %struct ObjectTrackFilter
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{ObjectTrackV4} Object Track.
     */
    reg("rest/v{4-}/analytics/objectTracks/:id?");

    /**%apidoc GET /rest/v{3-}/orderer
     * %ingroup Test
     * %return:{ArrayOrdererTestResponse}
     */
    reg("rest/v{3-}/orderer");
}

TEST_P(OpenApiSchemaTest, Validate)
{
    QJsonObject keyValue({{QString("name"), QJsonValue("name")}});

    {
        keyValue["value"] = QJsonValue(1);
        keyValue["unused"] = QJsonValue(815);
        std::unique_ptr<Request> request =
            restRequest({"POST /rest/{version}/settings HTTP/1.1"}, QJson::serialized(keyValue));
        http::HttpHeaders headers{};
        m_schemas->validateOrThrow(request.get(), &headers);
        EXPECT_TRUE(headers.contains("Warning"));
        EXPECT_EQ(headers.find("Warning")->second, "199 - \"Unused parameter: 'unused'\"");

        if (request->apiVersion() && *request->apiVersion() > 2)
        {
            EXPECT_FALSE(request->param("unused"));
        }
    }


    keyValue["value"] = QJsonValue(1);
    m_schemas->validateOrThrow(
        restRequest({"POST /rest/{version}/settings HTTP/1.1"}, QJson::serialized(keyValue)).get());
    keyValue["value"] = QJsonValue("1");
    m_schemas->validateOrThrow(
        restRequest({"POST /rest/{version}/settings HTTP/1.1"}, QJson::serialized(keyValue)).get());
    keyValue["value"] = QJsonArray({QJsonValue(1)});
    m_schemas->validateOrThrow(
        restRequest({"POST /rest/{version}/settings HTTP/1.1"}, QJson::serialized(keyValue)).get());
    keyValue["value"] = QJsonObject({{QString("value"), QJsonValue(1)}});
    m_schemas->validateOrThrow(
        restRequest({"POST /rest/{version}/settings HTTP/1.1"}, QJson::serialized(keyValue)).get());
    keyValue["value"] = QJsonValue(true);
    m_schemas->validateOrThrow(
        restRequest({"POST /rest/{version}/settings HTTP/1.1"}, QJson::serialized(keyValue)).get());
    keyValue["value"] = QJsonValue();
    m_schemas->validateOrThrow(
        restRequest({"POST /rest/{version}/settings HTTP/1.1"}, QJson::serialized(keyValue)).get());

    m_schemas->validateOrThrow(
        restRequest({"POST /rest/{version}/devices HTTP/1.1"}, QJson::serialized(TestDeviceModel())).get());
    m_schemas->validateOrThrow(
        restRequest({"GET /rest/{version}/layouts?_format=json&_keepDefault HTTP/1.1"}).get());
    m_schemas->validateOrThrow(restRequest({"PATCH /rest/{version}/devices/{id} HTTP/1.1"},
        R"json({"_orderBy": "attributes.scheduleTasks[].startTime", "id": "id"})json").get());
    ASSERT_THROW(
        try
        {
            m_schemas->validateOrThrow(
                restRequest({"GET /rest/{version}/layouts?_keepDefault&_unknown HTTP/1.1"}).get());
        }
        catch (const Exception& e)
        {
            ASSERT_EQ(e.message(), "Unknown parameter `_unknown`.");
            throw;
        },
        Exception);
    ASSERT_THROW(
        try
        {
            m_schemas->validateOrThrow(restRequest(
                {"POST /rest/{version}/devices HTTP/1.1"},
                R"json({
                    "general": {
                        "name": 1
                    }
                })json").get());
        }
        catch (const Exception& e)
        {
            ASSERT_EQ(e.message(), "`general.name` must be a string.");
            throw;
        },
        Exception);
    ASSERT_THROW(
        try
        {
            m_schemas->validateOrThrow(restRequest(
                {"POST /rest/{version}/devices HTTP/1.1"},
                R"json({
                    "general": {
                        "name": null
                    }
                })json").get());
        }
        catch (const Exception& e)
        {
            ASSERT_EQ(e.message(), "Invalid parameter `general.name`: null");
            throw;
        },
        Exception);
    ASSERT_THROW(
        try
        {
            m_schemas->validateOrThrow(restRequest({"PATCH /rest/{version}/devices/{id} HTTP/1.1"},
                R"json({"_orderBy": "invalid", "id": "id"})json").get());
        }
        catch (const Exception& e)
        {
            ASSERT_EQ(e.message(), "Invalid parameter `_orderBy`: invalid");
            throw;
        },
        Exception);
    ASSERT_THROW(
        try
        {
            m_schemas->validateOrThrow(
                restRequest({"PUT /rest/{version}/map HTTP/1.1"}, "{\"1\":{\"name\":1}}").get());
        }
        catch (const Exception& e)
        {
            ASSERT_EQ(e.message().toStdString(), "`1.name` must be a string.");
            throw;
        },
        Exception);
    ASSERT_THROW(
        try
        {
            m_schemas->validateOrThrow(
                restRequest({"PUT /rest/{version}/map HTTP/1.1"}, "{\"2\":{\"list\":[]}}").get());
        }
        catch (const Exception& e)
        {
            ASSERT_EQ(e.message().toStdString(), "Missing required parameter: 2.name.");
            throw;
        },
        Exception);
}

TEST_P(OpenApiSchemaTest, Postprocess)
{
    std::vector<TestDeviceModel> devices(1);
    devices[0].attributes = nx::vms::api::CameraAttributesData();
    devices[0].attributes->cameraId = nx::Uuid::createUuid();
    const auto request{restRequest({"GET /rest/{version}/devices HTTP/1.1"})};
    if (GetParam() == *kRestApiV3)
    {
        using namespace nx::utils::json;
        QJsonValue json;
        QJson::serialize(devices, &json);
        postprocessResponse(*request, &json);
        ASSERT_FALSE(getObject(asArray(json)[0], "attributes").contains("cameraId"));
    }
    else
    {
        using namespace nx::utils::reflect;
        auto json{nx::utils::serialization::json::serialized(devices, /*stripDefault*/ false)};
        postprocessResponse(*request, &json);
        ASSERT_FALSE(getObject(asArray(json)[0], "attributes").HasMember("cameraId"));
    }
}

TEST_P(OpenApiSchemaTest, ArrayOrdererVariant)
{
    ArrayOrdererTestResponse data{{{"key", {{{2}, {1}}}}}};
    const auto request{
        restRequest({"GET /rest/{version}/orderer?_orderBy=map.*[].id.%231 HTTP/1.1"})};
    if (GetParam() == *kRestApiV3)
    {
        using namespace nx::utils::json;
        QJsonValue json;
        QJson::serialize(data, &json);
        postprocessResponse(*request, &json);
        auto sorted{getArray(getObject(asObject(json), "map"), "key")};
        ASSERT_EQ(sorted.size(), 2);
        ASSERT_EQ(getDouble(asObject(sorted[0]), "id"), 1);
        ASSERT_EQ(getDouble(asObject(sorted[1]), "id"), 2);
    }
    else
    {
        using namespace nx::utils::reflect;
        detail::orderBy(&data, request->params());
        auto json{nx::utils::serialization::json::serialized(data, /*stripDefault*/ false)};
        postprocessResponse(*request, &json);
        auto sorted{getArray(getObject(asObject(json), "map"), "key")};
        ASSERT_EQ(sorted.Size(), 2);
        ASSERT_EQ(getDouble(asObject(sorted[0]), "id"), 1);
        ASSERT_EQ(getDouble(asObject(sorted[1]), "id"), 2);
    }
}

TEST_P(OpenApiSchemaTest, ArrayOrdererVariantNested)
{
    ArrayOrdererTestResponse data{
        {{"key", {{{ArrayOrderedTestNested{"2"}}, {ArrayOrderedTestNested{"1"}}}}}}};
    const auto request{
        restRequest({"GET /rest/{version}/orderer?_orderBy=map.*[].id.%232.name HTTP/1.1"})};
    if (GetParam() == *kRestApiV3)
    {
        using namespace nx::utils::json;
        QJsonValue json;
        QJson::serialize(data, &json);
        postprocessResponse(*request, &json);
        auto sorted{getArray(getObject(asObject(json), "map"), "key")};
        ASSERT_EQ(sorted.size(), 2);
        ASSERT_EQ(getString(getObject(asObject(sorted[0]), "id"), "name"), "1");
        ASSERT_EQ(getString(getObject(asObject(sorted[1]), "id"), "name"), "2");
    }
    else
    {
        using namespace nx::utils::reflect;
        detail::orderBy(&data, request->params());
        auto json{nx::utils::serialization::json::serialized(data, /*stripDefault*/ false)};
        postprocessResponse(*request, &json);
        auto sorted{getArray(getObject(asObject(json), "map"), "key")};
        ASSERT_EQ(sorted.Size(), 2);
        ASSERT_EQ(getString(getObject(asObject(sorted[0]), "id"), "name"), "1");
        ASSERT_EQ(getString(getObject(asObject(sorted[1]), "id"), "name"), "2");
    }
}

static const char* kTestJson = R"json([
    {"id":"44444444-4444-4444-4444-444444444444", "name":"4",
        "list":[
            {"id":"22222222-2222-2222-2222-222222222222", "name":"2"},
            {"id":"11111111-1111-1111-1111-111111111111", "name":"1"}
        ]
    },
    {"id":"33333333-3333-3333-3333-333333333333", "name":"3"}
])json";

TEST_P(OpenApiSchemaTest, EmptyOrderBy)
{
    const auto request{restRequest({"GET /rest/{version}/test?_orderBy= HTTP/1.1"})};
    if (GetParam() == *kRestApiV3)
    {
        using namespace nx::utils::json;
        QJsonValue json;
        QJson::deserialize(QByteArray{kTestJson}, &json);
        postprocessResponse(*request, &json);
        ASSERT_EQ(asArray(json).size(), 2);
        ASSERT_EQ(getString(asObject(asArray(json)[0]), "name"), "4");
        ASSERT_EQ(getArray(asObject(asArray(json)[0]), "list").size(), 2);
        ASSERT_EQ(
            getString(asObject(getArray(asObject(asArray(json)[0]), "list")[0]), "name"), "2");
        ASSERT_EQ(getString(asObject(asArray(json)[1]), "name"), "3");
    }
    else
    {
        using namespace nx::utils::reflect;
        auto [data, result] = nx::reflect::json::deserialize<std::vector<TestData>>(kTestJson);
        ASSERT_TRUE(result);
        detail::orderBy(&data, request->params());
        auto json{nx::utils::serialization::json::serialized(data, /*stripDefault*/ false)};
        postprocessResponse(*request, &json);
        ASSERT_EQ(asArray(json).Size(), 2);
        ASSERT_EQ(getString(asObject(asArray(json)[0]), "name"), "4");
        ASSERT_EQ(getArray(asObject(asArray(json)[0]), "list").Size(), 2);
        ASSERT_EQ(
            getString(asObject(getArray(asObject(asArray(json)[0]), "list")[0]), "name"), "2");
        ASSERT_EQ(getString(asObject(asArray(json)[1]), "name"), "3");
    }
}

TEST_P(OpenApiSchemaTest, OrderByNameAndListName)
{
    const auto request{
        restRequest({"GET /rest/{version}/test?_orderBy=name&_orderBy=list[].name HTTP/1.1"})};
    if (GetParam() == *kRestApiV3)
    {
        using namespace nx::utils::json;
        QJsonValue json;
        QJson::deserialize(QByteArray{kTestJson}, &json);
        postprocessResponse(*request, &json);
        ASSERT_EQ(asArray(json).size(), 2);
        ASSERT_EQ(getString(asObject(asArray(json)[0]), "name"), "3");
        ASSERT_EQ(getArray(asObject(asArray(json)[1]), "list").size(), 2);
        ASSERT_EQ(
            getString(asObject(getArray(asObject(asArray(json)[1]), "list")[0]), "name"), "1");
    }
    else
    {
        using namespace nx::utils::reflect;
        auto [data, result] = nx::reflect::json::deserialize<std::vector<TestData>>(kTestJson);
        ASSERT_TRUE(result);
        detail::orderBy(&data, request->params());
        auto json{nx::utils::serialization::json::serialized(data, /*stripDefault*/ false)};
        postprocessResponse(*request, &json);
        ASSERT_EQ(asArray(json).Size(), 2);
        ASSERT_EQ(getString(asObject(asArray(json)[0]), "name"), "3");
        ASSERT_EQ(getArray(asObject(asArray(json)[1]), "list").Size(), 2);
        ASSERT_EQ(
            getString(asObject(getArray(asObject(asArray(json)[1]), "list")[0]), "name"), "1");
    }
}

static const QByteArray kPostLayoutDataJson = R"json({
    "name": "name",
    "items": [],
    "fixedWidth": 0,
    "fixedHeight": 0
})json";

TEST_P(OpenApiSchemaTest, PerformancePost)
{
    HandlerPool pool;
    pool.registerHandler("ec2/saveLayout", new MockHandler<LayoutData>());
    testPerformance(&pool, {"POST /ec2/saveLayout HTTP/1.1"}, kPostLayoutDataJson);
}

TEST_P(OpenApiSchemaTest, PerformanceGet)
{
    HandlerPool pool;
    pool.registerHandler("ec2/getLayouts", new MockHandler<LayoutData>());
    testPerformance(&pool, {"GET /ec2/getLayouts HTTP/1.1"});
}

TEST_P(OpenApiSchemaTest, PerformanceCrudPost)
{
    HandlerPool pool;
    pool.setSchemas(m_schemas);
    pool.registerHandler(PathRouter::replaceVersionWithRegex("rest/v{3-}/layouts/:id?"),
        GlobalPermission::none, std::make_unique<MockCrudHandler<LayoutData>>());
    testPerformance(&pool, {"POST /rest/{version}/layouts HTTP/1.1"}, kPostLayoutDataJson);
}

TEST_P(OpenApiSchemaTest, PerformanceCrudGet)
{
    HandlerPool pool;
    pool.setSchemas(m_schemas);
    pool.registerHandler(PathRouter::replaceVersionWithRegex("rest/v{3-}/layouts/:id?"),
        GlobalPermission::none, std::make_unique<MockCrudHandler<LayoutData>>());
    testPerformance(
        &pool, {lit("GET /rest/{version}/layouts/%1 HTTP/1.1").arg(nx::Uuid::createUuid().toString())});
}

TEST_P(OpenApiSchemaTest, XmlResponse)
{
    HandlerPool pool;
    pool.setSchemas(m_schemas);
    pool.registerHandler(PathRouter::replaceVersionWithRegex("rest/v{3-}/layouts"),
        GlobalPermission::none, std::make_unique<MockCrudHandler<LayoutData>>());
    std::unique_ptr<Request> request =
        restRequest({"GET /rest/{version}/layouts?_keepDefault HTTP/1.1", "Accept: application/xml"});
    ASSERT_EQ(request->responseFormatOrThrow(), Qn::SerializationFormat::xml);
    auto handler = pool.findHandlerOrThrow(request.get());
    ASSERT_TRUE(handler);
    Response response = handler->executeRequestOrThrow(request.get());
    ASSERT_EQ(response.statusCode, http::StatusCode::ok);
    ASSERT_EQ(Qn::serializationFormatFromHttpContentType(response.content->type.toString()),
        Qn::SerializationFormat::xml);
    const QByteArray expected = GetParam() == *kRestApiV3
        ?
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<reply>"
                    "<element>"
                        "<backgroundHeight>0</backgroundHeight>"
                        "<backgroundImageFilename></backgroundImageFilename>"
                        "<backgroundOpacity>0.69999998807907104</backgroundOpacity>"
                        "<backgroundWidth>0</backgroundWidth>"
                        "<cellAspectRatio>0</cellAspectRatio>"
                        "<cellSpacing>0.05000000074505806</cellSpacing>"
                        "<fixedHeight>0</fixedHeight>"
                        "<fixedWidth>0</fixedWidth>"
                        "<id>{00000000-0000-0000-0000-000000000000}</id>"
                        "<items/>"
                        "<locked>false</locked>"
                        "<logicalId>0</logicalId>"
                        "<name>\u0414\u043e\u043c</name>"
                        "<parentId>{00000000-0000-0000-0000-000000000000}</parentId>"
                    "</element>"
                "</reply>\n"
        :
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<reply>"
                    "<element>"
                        "<id>{00000000-0000-0000-0000-000000000000}</id>"
                        "<parentId>{00000000-0000-0000-0000-000000000000}</parentId>"
                        "<name>\u0414\u043e\u043c</name>"
                        "<cellAspectRatio>0</cellAspectRatio>"
                        "<cellSpacing>0.05000000074505806</cellSpacing>"
                        "<items/>"
                        "<locked>false</locked>"
                        "<backgroundImageFilename></backgroundImageFilename>"
                        "<backgroundWidth>0</backgroundWidth>"
                        "<backgroundHeight>0</backgroundHeight>"
                        "<backgroundOpacity>0.69999998807907104</backgroundOpacity>"
                        "<fixedWidth>0</fixedWidth>"
                        "<fixedHeight>0</fixedHeight>"
                        "<logicalId>0</logicalId>"
                    "</element>"
                "</reply>\n";
    ASSERT_EQ(response.content->body, expected);
}

TEST_P(OpenApiSchemaTest, CsvResponse)
{
    HandlerPool pool;
    pool.setSchemas(m_schemas);
    pool.registerHandler(PathRouter::replaceVersionWithRegex("rest/v{3-}/layouts"),
        GlobalPermission::none, std::make_unique<MockCrudHandler<LayoutData>>());
    std::unique_ptr<Request> request =
        restRequest({"GET /rest/{version}/layouts?_keepDefault HTTP/1.1", "Accept: text/csv"});
    ASSERT_EQ(request->responseFormatOrThrow(), Qn::SerializationFormat::csv);
    auto handler = pool.findHandlerOrThrow(request.get());
    ASSERT_TRUE(handler);
    Response response = handler->executeRequestOrThrow(request.get());
    ASSERT_EQ(response.statusCode, http::StatusCode::ok);
    ASSERT_EQ(Qn::serializationFormatFromHttpContentType(response.content->type.toString()),
        Qn::SerializationFormat::csv);
    const QByteArray expected = GetParam() == *kRestApiV3
        ?
            "backgroundHeight,"
            "backgroundImageFilename,"
            "backgroundOpacity,"
            "backgroundWidth,"
            "cellAspectRatio,"
            "cellSpacing,"
            "fixedHeight,"
            "fixedWidth,"
            "id,"
            "locked,"
            "logicalId,"
            "name,"
            "parentId"
            "\r\n"
            "0,"
            ","
            "0.69999998807907104,"
            "0,"
            "0,"
            "0.05000000074505806,"
            "0,"
            "0,"
            "{00000000-0000-0000-0000-000000000000},"
            "false,"
            "0,"
            "\u0414\u043e\u043c,"
            "{00000000-0000-0000-0000-000000000000}"
            "\r\n"
        :
            "id,"
            "parentId,"
            "name,"
            "cellAspectRatio,"
            "cellSpacing,"
            "locked,"
            "backgroundImageFilename,"
            "backgroundWidth,"
            "backgroundHeight,"
            "backgroundOpacity,"
            "fixedWidth,"
            "fixedHeight,"
            "logicalId"
            "\r\n"
            "{00000000-0000-0000-0000-000000000000},"
            "{00000000-0000-0000-0000-000000000000},"
            "\u0414\u043e\u043c,"
            "0,"
            "0.05000000074505806,"
            "false,"
            ","
            "0,"
            "0,"
            "0.69999998807907104,"
            "0,"
            "0,"
            "0"
            "\r\n";
    ASSERT_EQ(response.content->body, expected);
}

TEST_P(OpenApiSchemaTest, PrettyResponse)
{
    HandlerPool pool;
    pool.setSchemas(m_schemas);
    pool.registerHandler(PathRouter::replaceVersionWithRegex("rest/v{3-}/layouts"),
        GlobalPermission::none, std::make_unique<MockCrudHandler<LayoutData>>());
    std::unique_ptr<Request> request = restRequest(
        {"GET /rest/{version}/layouts?_keepDefault&_pretty HTTP/1.1", "Accept: application/json"});
    auto handler = pool.findHandlerOrThrow(request.get());
    ASSERT_TRUE(handler);
    Response response = handler->executeRequestOrThrow(request.get());
    ASSERT_TRUE(response.content);
    ASSERT_EQ(
        response.content->body.toStdString(),
        GetParam() == *kRestApiV3
            ?
                R"json([
    {
        "backgroundHeight": 0,
        "backgroundImageFilename": "",
        "backgroundOpacity": 0.699999988079071,
        "backgroundWidth": 0,
        "cellAspectRatio": 0,
        "cellSpacing": 0.05000000074505806,
        "fixedHeight": 0,
        "fixedWidth": 0,
        "id": "{00000000-0000-0000-0000-000000000000}",
        "items": [],
        "locked": false,
        "logicalId": 0,
        "name": ")json" "\u0414\u043e\u043c" R"json(",
        "parentId": "{00000000-0000-0000-0000-000000000000}"
    }
])json"
            :
                R"json([
    {
        "id": "{00000000-0000-0000-0000-000000000000}",
        "parentId": "{00000000-0000-0000-0000-000000000000}",
        "name": ")json" "\u0414\u043e\u043c" R"json(",
        "cellAspectRatio": 0.0,
        "cellSpacing": 0.05000000074505806,
        "items": [],
        "locked": false,
        "backgroundImageFilename": "",
        "backgroundWidth": 0,
        "backgroundHeight": 0,
        "backgroundOpacity": 0.699999988079071,
        "fixedWidth": 0,
        "fixedHeight": 0,
        "logicalId": 0
    }
])json"
    );
}

TEST_P(OpenApiSchemaTest, Base64)
{
    HandlerPool pool;
    pool.setSchemas(m_schemas);
    pool.registerHandler(PathRouter::replaceVersionWithRegex("rest/v{3-}/base64/:param/echo"),
        GlobalPermission::none, std::make_unique<MockBase64Handler>());
    std::unique_ptr<Request> request =
        restRequest({"GET /rest/{version}/base64/h4%2FdspcBduhE3AAkLjK9Jg%3D%3D/echo HTTP/1.1"});
    auto handler = pool.findHandlerOrThrow(request.get());
    ASSERT_TRUE(handler);
    Response response = handler->executeRequestOrThrow(request.get());
    ASSERT_EQ(response.statusCode, http::StatusCode::ok);
    ASSERT_EQ(response.content->body, "{\"param\":\"h4/dspcBduhE3AAkLjK9Jg==\"}");
}

TEST_P(OpenApiSchemaTest, Map)
{
    HandlerPool pool;
    pool.setSchemas(m_schemas);
    pool.registerHandler(PathRouter::replaceVersionWithRegex("rest/v{3-}/map"),
        GlobalPermission::none, std::make_unique<MockMapHandler>());
    std::unique_ptr<Request> requestPut = restRequest(
        {"PUT /rest/{version}/map"},
        /*suppress newline*/ 1 + (const char*)
R"json(
{
    "{00000000-0000-0000-0000-000000000000}": {
        "list": [
            {
                "id": "{22222222-2222-2222-2222-222222222222}",
                "name": "2"
            },
            {
                "id": "{11111111-1111-1111-1111-111111111111}",
                "name": "1"
            }
        ],
        "name": "0"
    }
})json");
    auto handler = pool.findHandlerOrThrow(requestPut.get());
    ASSERT_TRUE(handler);
    Response response = handler->executeRequestOrThrow(requestPut.get());
    ASSERT_EQ(response.statusCode, http::StatusCode::ok);
    std::unique_ptr<Request> requestGet =
        restRequest({"GET /rest/{version}/map?_orderBy=*.list[].name HTTP/1.1"});
    response = handler->executeRequestOrThrow(requestGet.get());
    ASSERT_EQ(response.statusCode, http::StatusCode::ok);
    ASSERT_EQ(
        nx::utils::formatJsonString(response.content->body).toStdString(),
        /*suppress newline*/ 1 + (const char*)
R"json(
{
    "{00000000-0000-0000-0000-000000000000}": {
        "list": [
            {
                "id": "{11111111-1111-1111-1111-111111111111}",
                "name": "1"
            },
            {
                "id": "{22222222-2222-2222-2222-222222222222}",
                "name": "2"
            }
        ],
        "name": "0"
    }
})json");

    requestGet = restRequest({"GET /rest/{version}/map?_with=*.list.name HTTP/1.1"});
    handler = pool.findHandlerOrThrow(requestGet.get());
    ASSERT_TRUE(handler);
    response = handler->executeRequestOrThrow(requestGet.get());
    ASSERT_EQ(response.statusCode, http::StatusCode::ok);
    ASSERT_EQ(
        nx::utils::formatJsonString(response.content->body).toStdString(),
        /*suppress newline*/ 1 + (const char*)
R"json(
{
    "{00000000-0000-0000-0000-000000000000}": {
        "list": [
            {
                "name": "2"
            },
            {
                "name": "1"
            }
        ]
    }
})json");

    std::unique_ptr<Request> requestGetFiltered = restRequest(
        {"GET /rest/{version}/map?{00000000-0000-0000-0000-000000000000}.list.name=1 HTTP/1.1"});
    handler = pool.findHandlerOrThrow(requestGetFiltered.get());
    ASSERT_TRUE(handler);
    response = handler->executeRequestOrThrow(requestGetFiltered.get());
    ASSERT_EQ(response.statusCode, http::StatusCode::ok);
    ASSERT_EQ(
        nx::utils::formatJsonString(response.content->body).toStdString(),
        /*suppress newline*/ 1 + (const char*)
R"json(
{
    "{00000000-0000-0000-0000-000000000000}": {
        "list": [
            {
                "id": "{11111111-1111-1111-1111-111111111111}",
                "name": "1"
            }
        ],
        "name": "0"
    }
})json");
}

INSTANTIATE_TEST_SUITE_P(Rest, OpenApiSchemaTest, ::testing::ValuesIn(kRestApiV3, kRestApiEnd),
    [](auto info) { return std::string(info.param); });

} // namespace nx::network::rest::json::test
