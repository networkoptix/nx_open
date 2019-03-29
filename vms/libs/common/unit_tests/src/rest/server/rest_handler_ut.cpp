#include <gtest/gtest.h>

#include <rest/server/request_handler.h>

class RequestParamsTest:
    public ::testing::Test
{
public:
    static void testTo(
        const RequestParams& params,
        const QByteArray& url, const QByteArray& json)
    {
        EXPECT_EQ(url, params.toUrlQuery().toString());
        EXPECT_EQ(json, QJsonDocument(params.toJson()).toJson(QJsonDocument::Compact));
    }

    static void testFrom(
        const RequestParams& params,
        const QByteArray& url, const QByteArray& json = {})
    {
        const auto fromUrl = params.fromUrlQuery(QUrlQuery(url));
        EXPECT_EQ(params, fromUrl) << containerString(fromUrl).toStdString();

        if (!json.isEmpty())
        {
            const auto fromJson = params.fromJson(QJsonDocument::fromJson(json).object());
            EXPECT_EQ(params, fromJson) << containerString(fromJson).toStdString();
        }
    }

    static void testConversions(
        const RequestParams& params,
        const QByteArray& url, const QByteArray& json)
    {
        testTo(params, url, json);
        testFrom(params, url, json);
    }
};

TEST_F(RequestParamsTest, SimpleConversions)
{
    testConversions(
        {{"a", "1"}, {"b", "hello"}, {"c", "true"}},
        "a=1&b=hello&c=true",
        R"json({"a":1,"b":"hello","c":true})json");
}

TEST_F(RequestParamsTest, DuplicateConversions)
{
    testTo(
        {{"a", "1"}, {"b", "hello"}, {"b", "world"}, {"c", "true"}},
        "a=1&b=world&b=hello&c=true",
        R"json({"a":1,"b":"hello","c":true})json");

    testFrom(
        {{"a", "1"}, {"b", "hello"}, {"b", "world"}, {"c", "true"}},
        "a=1&b=hello&b=world&c=true");
}

TEST_F(RequestParamsTest, SpecialCharactersConversions)
{
    testConversions(
        {{"a", "?"}, {"b", "&"}, {"c", "\""}},
        "a=?&b=%26&c=%22",
        R"json({"a":"?","b":"&","c":"\""})json");
}

class RestRequestTest:
    public ::testing::Test
{
public:
    std::optional<RestRequest> restRequest(const QStringList& text)
    {
        auto httpRequest = std::make_unique<nx::network::http::Request>();
        if (!httpRequest->parse(text.join("\r\n").toUtf8()))
            return std::nullopt;

        RestRequest request(httpRequest.get(), nullptr);
        auto contentType = nx::network::http::getHeaderValue(httpRequest->headers, "Content-Type");
        if (!contentType.isEmpty())
            request.content = {std::move(contentType), std::move(httpRequest->messageBody)};

        m_requests.push_back(std::move(httpRequest));
        return request;
    }

private:
    std::vector<std::unique_ptr<nx::network::http::Request>> m_requests;
};

struct SomeData
{
    bool b = false;
    int i = 0;
    QString s;
};

#define SomeData_Fields (b)(i)(s)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((SomeData), (eq)(json), _Fields);

static void PrintTo(const SomeData& value, std::ostream* stream)
{
    *stream << QJson::serialized(value).toStdString();
}

TEST_F(RestRequestTest, Get)
{
    const auto request = restRequest({
        "GET /some/path HTTP/1.1",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("GET", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams{}, request->params());
    EXPECT_FALSE(request->content);
    EXPECT_FALSE(request->isExtraFormattingRequired());
}

TEST_F(RestRequestTest, GetWithUrlParams)
{
    const auto request = restRequest({
        "GET /some/path?b=true&i=42&s=hi&extraFormatting HTTP/1.1",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("GET", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_TRUE(request->isExtraFormattingRequired());
    EXPECT_EQ(
        RequestParams({{"b", "true"}, {"extraFormatting", ""}, {"i", "42"}, {"s", "hi"}}),
        request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{true, 42, "hi"}), *data);
}

TEST_F(RestRequestTest, GetToPostByHeader)
{
    const auto request = restRequest({
        "GET /some/path?b=true&i=777&s=data HTTP/1.1",
        "X-Method-Override: post",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams({{"b", "true"}, {"i", "777"}, {"s", "data"}}), request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{true, 777, "data"}), *data);
}

TEST_F(RestRequestTest, GetToPutByParam)
{
    // TODO: setIniValue(&nx::network::rest::ini().allowGetMethodReplacement, true);

    const auto request = restRequest({
        "GET /some/path?s=someData&method_=put HTTP/1.1",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("PUT", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams({{"method_", "put"}, {"s", "someData"}}), request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{false, 0, "someData"}), *data);
}

// TODO: Enable when ini config works properly.
TEST_F(RestRequestTest, DISABLED_GetToPutByParamFailure)
{
    // TODO: setIniValue(&nx::network::rest::ini().allowGetMethodReplacement, false);

    const auto request = restRequest({
        "GET /some/path?s=someData&method_=put HTTP/1.1",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("GET", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams({{"method_", "put"}, {"s", "someData"}}), request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{false, 0, "someData"}), *data);
}

TEST_F(RestRequestTest, Post)
{
    const auto request = restRequest({
        "POST /some/path HTTP/1.1",
        "Content-Length: 0",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams(), request->params());

    EXPECT_FALSE(request->parseContent<SomeData>());
}

TEST_F(RestRequestTest, PostJson)
{
    const auto request = restRequest({
        "POST /some/path HTTP/1.1",
        "Content-Type: application/json",
        "Content-Length: 29",
        "",
        R"json({"b":true,"i":222,"s":"data"})json"
    });
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->content);
    EXPECT_EQ("application/json", request->content->type);
    EXPECT_EQ(R"json({"b":true,"i":222,"s":"data"})json", request->content->body);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams({{"b", "true"}, {"i", "222"}, {"s", "data"}}), request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{true, 222, "data"}), *data);
}

TEST_F(RestRequestTest, PostForm)
{
    const auto request = restRequest({
        "POST /some/path HTTP/1.1",
        "Content-Type: application/x-www-form-urlencoded",
        "Content-Length: 27",
        "",
        "b=false&i=123&s=some%20text"
    });
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->content);
    EXPECT_EQ("application/x-www-form-urlencoded", request->content->type);
    EXPECT_EQ("b=false&i=123&s=some%20text", request->content->body);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams({{"b", "false"}, {"i", "123"}, {"s", "some text"}}), request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{false, 123, "some text"}), *data);
}

TEST_F(RestRequestTest, PostBadJson)
{
    const auto request = restRequest({
        "POST /some/path HTTP/1.1",
        "Content-Type: application/json",
        "Content-Length: 29",
        "",
        "not a json",
    });
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->content);
    EXPECT_EQ("application/json", request->content->type);
    EXPECT_EQ("not a json", request->content->body);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams(), request->params());

    ASSERT_FALSE(request->parseContent<SomeData>());
}

TEST_F(RestRequestTest, PostWithUrlParams)
{
    // TODO: setIniValue(&nx::network::rest::ini().allowUrlParamitersForAnyMethod, true);

    const auto request = restRequest({
        "POST /some/path?b=true&i=42&s=hi HTTP/1.1",
        "Content-Length: 0",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams({{"b", "true"}, {"i", "42"}, {"s", "hi"}}), request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{true, 42, "hi"}), *data);
}

// TODO: Enable when ini config works properly.
TEST_F(RestRequestTest, DISABLED_PostWithUrlParamsFailure)
{
    // TODO: setIniValue(&nx::network::rest::ini().allowUrlParamitersForAnyMethod, false);

    const auto request = restRequest({
        "POST /some/path?b=true&i=42&s=hi HTTP/1.1",
        "Content-Length: 0",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams(), request->params());
    ASSERT_FALSE(request->parseContent<SomeData>());
}

TEST_F(RestRequestTest, PutJson)
{
    const auto request = restRequest({
        "PUT /some/path HTTP/1.1",
        "Content-Type: application/json",
        "Content-Length: 17",
        "",
        R"json({"s":"some text"})json"
    });
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->content);
    EXPECT_EQ("application/json", request->content->type);
    EXPECT_EQ(R"json({"s":"some text"})json", request->content->body);

    EXPECT_EQ("PUT", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams({{"s", "some text"}}), request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{false, 0, "some text"}), *data);
}

TEST_F(RestRequestTest, Delete)
{
    const auto request = restRequest({
        "DELETE /some/path?id=ID HTTP/1.1",
        "",
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("DELETE", request->method());
    EXPECT_EQ("/some/path", request->path());
    EXPECT_EQ(RequestParams({{"id", "ID"}}), request->params());
}

TEST_F(RestRequestTest, RestException)
{
    const auto request = restRequest({
        "PUT /some/path HTTP/1.1",
        "Content-Type: application/json",
        "Content-Length: 10",
        "",
        "trash data"
    });
    ASSERT_TRUE(request);

    try
    {
        request->paramOrThrow("id");
        FAIL() << "did not throw";
    }
    catch (const RestException& error)
    {
        EXPECT_EQ(
            "Missing required parameter 'id'",
            error.descriptor.text());
    }

    try
    {
        request->parseContentOrThrow<SomeData>();
        FAIL() << "did not throw";
    }
    catch (const RestException& error)
    {
        EXPECT_EQ(
            "Failed to process request 'Unable to parse request of SomeData'",
            error.descriptor.text());
    }
}

class RestResponseTest:
    public ::testing::Test
{
public:
    static void expectJsonResponse(
        const RestResponse& response,
        nx::network::http::StatusCode::Value statusCode,
        const QByteArray& json)
    {
        EXPECT_EQ(statusCode, response.statusCode);
        ASSERT_TRUE(response.content);
        EXPECT_EQ("application/json", response.content->type);
        EXPECT_EQ(json, response.content->body);
    }
};

TEST_F(RestResponseTest, JsonResult)
{
    QnJsonRestResult result;
    result.setReply(SomeData{false, 111, "hello"});
    expectJsonResponse(
        RestResponse::result(result),
        nx::network::http::StatusCode::ok,
        R"json({"error":"0","errorString":"","reply":{"b":false,"i":111,"s":"hello"}})json");
}

TEST_F(RestResponseTest, JsonError)
{
    QnJsonRestResult result;
    result.setError({QnRestResult::MissingParameter, "name"});
    expectJsonResponse(
        RestResponse::result(result),
        nx::network::http::StatusCode::unprocessableEntity,
        R"json({"error":"1","errorString":"Missing required parameter 'name'","reply":null})json");
}

TEST_F(RestResponseTest, RestError)
{
    QnRestResult result;
    result.setError(QnRestResult::InternalServerError, "Something went wrong");
    expectJsonResponse(
        RestResponse::result(result),
        nx::network::http::StatusCode::badRequest,
        R"json({"error":"6","errorString":"Something went wrong","reply":null})json");
}

TEST_F(RestResponseTest, DataReply)
{
    expectJsonResponse(
        RestResponse::reply(SomeData{true, 55, "ok"}),
        nx::network::http::StatusCode::ok,
        R"json({"error":"0","errorString":"","reply":{"b":true,"i":55,"s":"ok"}})json");
}

TEST_F(RestResponseTest, ErrorResult)
{
    expectJsonResponse(
        RestResponse::error(QnRestResult::InvalidParameter, "a", "X"),
        nx::network::http::StatusCode::unprocessableEntity,
        R"json({"error":"2","errorString":"Invalid parameter 'a' value: 'X'","reply":null})json");
}
