// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/kit/ini_config.h>
#include <nx/network/rest/handler.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/utils/test_support/run_test.h>

namespace nx::network::rest::test {

class RestParamsTest: public ::testing::Test
{
public:
    static void testTo(
        const Params& params,
        const QByteArray& url, const QByteArray& json)
    {
        EXPECT_EQ(url, params.toUrlQuery().toString());
        EXPECT_EQ(json, QJsonDocument(params.toJson()).toJson(QJsonDocument::Compact));
    }

    static void testFrom(
        const Params& params,
        const QByteArray& url, const QByteArray& json = {})
    {
        const auto fromUrl = params.fromUrlQuery(QUrlQuery(url));
        EXPECT_EQ(params, fromUrl) << fromUrl.toString().toStdString();

        if (!json.isEmpty())
        {
            const auto fromJson = params.fromJson(QJsonDocument::fromJson(json).object());
            EXPECT_EQ(params, fromJson) << fromJson.toString().toStdString();
        }
    }

    static void testConversions(
        const Params& params,
        const QByteArray& url, const QByteArray& json)
    {
        testTo(params, url, json);
        testFrom(params, url, json);
    }
};

TEST_F(RestParamsTest, SimpleConversions)
{
    testConversions(
        {{"a", "1"}, {"b", "hello"}, {"c", "true"}},
        "a=1&b=hello&c=true",
        R"json({"a":"1","b":"hello","c":"true"})json");
}

TEST_F(RestParamsTest, DuplicateConversions)
{
    testTo(
        {{"a", "1"}, {"b", "hello"}, {"b", "world"}, {"c", "true"}},
        "a=1&b=world&b=hello&c=true",
        R"json({"a":"1","b":["world","hello"],"c":"true"})json");

    testFrom(
        {{"a", "1"}, {"b", "hello"}, {"b", "world"}, {"c", "true"}},
        "a=1&b=hello&b=world&c=true");
}

TEST_F(RestParamsTest, SpecialCharactersConversions)
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
    std::optional<rest::Request> restRequest(const QStringList& text)
    {
        auto httpRequest = std::make_unique<http::Request>();
        if (!httpRequest->parse(text.join("\r\n").toUtf8()))
            return std::nullopt;

        rest::Request request(httpRequest.get(), kSystemSession);
        auto contentType = http::getHeaderValue(httpRequest->headers, "Content-Type");
        if (!contentType.empty())
            request.content = {std::move(contentType), httpRequest->messageBody.toByteArray()};

        m_requests.push_back(std::move(httpRequest));
        return request;
    }

    nx::kit::IniConfig::Tweaks iniTweaks;

private:
    std::vector<std::unique_ptr<http::Request>> m_requests;
};

struct SomeData
{
    bool b = false;
    int i = 0;
    QString s;

    bool operator==(const SomeData& other) const = default;
};
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SomeData, (json), (b)(i)(s))

static void PrintTo(const SomeData& value, std::ostream* stream)
{
    *stream << QJson::serialized(value).toStdString();
}

struct SomeBools
{
    bool a = false;
    bool b = false;
    bool c = true;
    bool d = true;

    bool operator==(const SomeBools& other) const = default;
};
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SomeBools, (json), (a)(b)(c)(d))

static void PrintTo(const SomeBools& value, std::ostream* stream)
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
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params{}, request->params());
    EXPECT_FALSE(request->content);
    EXPECT_FALSE(request->isExtraFormattingRequired());
    EXPECT_FALSE(request->parseContent<SomeData>());
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
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_TRUE(request->isExtraFormattingRequired());
    EXPECT_EQ(
        Params({{"b", "true"}, {"extraFormatting", ""}, {"i", "42"}, {"s", "hi"}}),
        request->params());

    EXPECT_TRUE(request->param("extraFormatting"));
    EXPECT_FALSE(request->param("notExist"));

    EXPECT_EQ(std::optional<int>(42), request->param<int>("i"));
    EXPECT_FALSE(request->param<int>("b"));

    EXPECT_EQ("", request->paramOrDefault("notExist"));
    EXPECT_EQ("default", request->paramOrDefault("notExist", QString("default")));
    EXPECT_EQ(123, request->paramOrDefault<int>("b", 123));

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{true, 42, "hi"}), *data);
}

TEST_F(RestRequestTest, GetWithJsonInString)
{
    const auto request = restRequest({
        "GET /some/path?s={\"b\":true,\"i\":7,\"s\":\"hi\"} HTTP/1.1",
        ""
    });

    EXPECT_EQ("GET", request->method());
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params({{"s", R"json({"b":true,"i":7,"s":"hi"})json"}}), request->params());

    const auto innerData1 = QJson::deserialized<SomeData>(request->paramOrThrow("s").toUtf8());
    EXPECT_EQ((SomeData{true, 7, "hi"}), innerData1);

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);

    const auto innerData2 = QJson::deserialized<SomeData>(data->s.toUtf8());
    EXPECT_EQ((SomeData{true, 7, "hi"}), innerData2);
}

TEST_F(RestRequestTest, GetWithSomeBools)
{
    const auto request = restRequest({
        "GET /some/path?a=true&c=false HTTP/1.1",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);
    EXPECT_EQ(Params({{"a", "true"}, {"c", "false"}}), request->params());

    const auto data = request->parseContentOrThrow<SomeBools>();
    EXPECT_EQ((SomeBools{true, false, false, true}), data);
}

TEST_F(RestRequestTest, GetWithMixedBools)
{
    const auto request = restRequest({
        "GET /some/path?a=1&b=True&c=0&d=False HTTP/1.1",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);
    EXPECT_EQ(Params({{"a", "1"}, {"b", "True"}, {"c", "0"}, {"d", "False"}}), request->params());

    const auto data = request->parseContentOrThrow<SomeBools>();
    EXPECT_EQ((SomeBools{true, true, false, false}), data);
}

TEST_F(RestRequestTest, GetWithBoolString)
{
    const auto request = restRequest({
        "GET /some/path?s=true HTTP/1.1",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);
    EXPECT_EQ(Params({{"s", "true"}}), request->params());

    const auto data = request->parseContentOrThrow<SomeData>();
    EXPECT_EQ((SomeData{false, 0, "true"}), data);
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
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params({{"b", "true"}, {"i", "777"}, {"s", "data"}}), request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{true, 777, "data"}), *data);
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
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params(), request->params());

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
    EXPECT_EQ(nx::network::http::header::ContentType::kJson, request->content->type);
    EXPECT_EQ(R"json({"b":true,"i":222,"s":"data"})json", request->content->body);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params({{"b", "true"}, {"i", "222"}, {"s", "data"}}), request->params());

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
    EXPECT_EQ(
        nx::network::http::header::ContentType("application/x-www-form-urlencoded"),
        request->content->type);
    EXPECT_EQ("b=false&i=123&s=some%20text", request->content->body);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params({{"b", "false"}, {"i", "123"}, {"s", "some text"}}), request->params());

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
    EXPECT_EQ(nx::network::http::header::ContentType::kJson, request->content->type);
    EXPECT_EQ("not a json", request->content->body);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params(), request->params());

    ASSERT_FALSE(request->parseContent<SomeData>());
}

TEST_F(RestRequestTest, PostWithJsonInString)
{
    const auto request = restRequest({
        "POST /some/path HTTP/1.1",
        "Content-Type: application/json",
        "Content-Length: 42",
        "",
        R"json({"s":"{\"b\":true,\"i\":7,\"s\":\"hi\"}"})json"
    });

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params({{"s", R"json({"b":true,"i":7,"s":"hi"})json"}}), request->params());

    const auto innerData1 = QJson::deserialized<SomeData>(request->paramOrThrow("s").toUtf8());
    EXPECT_EQ((SomeData{true, 7, "hi"}), innerData1);

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);

    const auto innerData2 = QJson::deserialized<SomeData>(data->s.toUtf8());
    EXPECT_EQ((SomeData{true, 7, "hi"}), innerData2);
}

TEST_F(RestRequestTest, PostWithEmbeddedJson)
{
    const auto request = restRequest({
        "POST /some/path HTTP/1.1",
        "Content-Type: application/json",
        "Content-Length: 32",
        "",
        R"json({"s":{"b":true,"i":7,"s":"hi"}})json"
    });

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params({{"s", R"json({"b":true,"i":7,"s":"hi"})json"}}), request->params());

    const auto innerData1 = QJson::deserialized<SomeData>(request->paramOrThrow("s").toUtf8());
    EXPECT_EQ((SomeData{true, 7, "hi"}), innerData1);

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
}

TEST_F(RestRequestTest, PostWithUrlParams)
{
    iniTweaks.set(&rest::ini().allowUrlParametersForAnyMethod, true);

    const auto request = restRequest({
        "POST /some/path?b=true&i=42&s=hi HTTP/1.1",
        "Content-Length: 0",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params({{"b", "true"}, {"i", "42"}, {"s", "hi"}}), request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{true, 42, "hi"}), *data);
}

TEST_F(RestRequestTest, PostWithUrlParamsFailure)
{
    iniTweaks.set(&rest::ini().allowUrlParametersForAnyMethod, false);

    const auto request = restRequest({
        "POST /some/path?b=true&i=42&s=hi HTTP/1.1",
        "Content-Length: 0",
        ""
    });
    ASSERT_TRUE(request);
    ASSERT_FALSE(request->content);

    EXPECT_EQ("POST", request->method());
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params(), request->params());
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
    EXPECT_EQ(nx::network::http::header::ContentType::kJson, request->content->type);
    EXPECT_EQ(R"json({"s":"some text"})json", request->content->body);

    EXPECT_EQ("PUT", request->method());
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params({{"s", "some text"}}), request->params());

    const auto data = request->parseContent<SomeData>();
    ASSERT_TRUE(data);
    EXPECT_EQ((SomeData{false, 0, "some text"}), *data);
}

TEST_F(RestRequestTest, PutJsonInsteadOfEncodedUrl)
{
    const auto request = restRequest({
        "PUT /some/path HTTP/1.1",
        "Content-Type: application/x-www-form-urlencoded",
        "Content-Length: 17",
        "",
        R"json({"s":"some text"})json"
    });
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->content);
    EXPECT_EQ(nx::network::http::header::ContentType::kForm, request->content->type);
    EXPECT_EQ(R"json({"s":"some text"})json", request->content->body);

    EXPECT_EQ("PUT", request->method());
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_NE(Params({{"s", "some text"}}), request->params());

    try
    {
        const auto data = request->parseContent<SomeData>();
        FAIL() << "did not throw";
    }
    catch(Exception &e)
    {
        EXPECT_EQ(e.message(), "Failed to parse request data");
    }
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
    EXPECT_EQ("/some/path", request->decodedPath());
    EXPECT_EQ(Params({{"id", "ID"}}), request->params());
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
    catch (const rest::Exception& error)
    {
        EXPECT_EQ("Missing required parameter: id.", error.message());
    }

    try
    {
        request->parseContentOrThrow<SomeData>();
        FAIL() << "did not throw";
    }
    catch (const rest::Exception& error)
    {
        EXPECT_EQ(error.message(), "Unable to parse a boolean from JSON: \"trash data\"");
    }
}

TEST_F(RestRequestTest, RestTypeException)
{
    const auto request = restRequest({
        "PUT /some/path?id=777 HTTP/1.1",
        "Content-Type: application/json",
        "Content-Length: 10",
        "",
        "trash data"
    });
    ASSERT_TRUE(request);

    try
    {
        request->paramOrThrow<bool>("id");
        FAIL() << "did not throw";
    }
    catch (const rest::Exception& error)
    {
        EXPECT_EQ("Invalid parameter `id`: 777", error.message());
    }

    ASSERT_EQ(777, request->paramOrThrow<int>("id"));
}

class RestResponseTest:
    public ::testing::Test
{
public:
    static void expectJsonResponse(
        const rest::Response& response, http::StatusCode::Value statusCode, const std::string& json)
    {
        EXPECT_EQ(statusCode, response.statusCode);
        ASSERT_TRUE(response.content);
        EXPECT_EQ(nx::network::http::header::ContentType::kJson, response.content->type);
        EXPECT_EQ(json, (std::string) response.content->body);
    }
};

TEST_F(RestResponseTest, JsonResult)
{
    JsonResult result;
    result.setReply(SomeData{false, 111, "hello"});
    expectJsonResponse(
        rest::Response::result(result),
        http::StatusCode::ok,
        R"json({"error":"0","errorId":"ok","errorString":"","reply":{"b":false,"i":111,"s":"hello"}})json");
}

TEST_F(RestResponseTest, JsonError)
{
    expectJsonResponse(
        rest::Response::result(JsonResult{Result::MissingParameter, "Error string"}),
        http::StatusCode::unprocessableEntity,
        R"json({"error":"1","errorId":"missingParameter","errorString":"Error string"})json");
}

TEST_F(RestResponseTest, RestError)
{
    expectJsonResponse(
        rest::Response(Result{Result::InternalServerError, "Something went wrong"}),
        http::StatusCode::internalServerError,
        R"json({"error":"6","errorId":"internalServerError","errorString":"Something went wrong"})json");
}

TEST_F(RestResponseTest, DataReply)
{
    expectJsonResponse(
        rest::Response::reply(SomeData{true, 55, "ok"}),
        http::StatusCode::ok,
        R"json({"error":"0","errorId":"ok","errorString":"","reply":{"b":true,"i":55,"s":"ok"}})json");
}

TEST_F(RestResponseTest, ErrorResult)
{
    expectJsonResponse(
        rest::Response(Result::invalidParameter("a", "X")),
        http::StatusCode::unprocessableEntity,
        R"json({"error":"2","errorId":"invalidParameter","errorString":"Invalid parameter `a`: X"})json");
}

class RestResultTest:
    public ::testing::Test
{
protected:
    void baseTest(const QByteArray& data, rest::Result::Error error, const QString& errorString = "")
    {
        NX_INFO(NX_SCOPE_TAG, data);
        const auto result = QJson::deserializeOrThrow<rest::Result>(data);
        EXPECT_EQ(result.error, error) << data.toStdString();
        EXPECT_EQ(result.errorString, errorString) << data.toStdString();
    }

    void jsonErrorTest(const QByteArray& data, rest::Result::Error error, const QString& errorString)
    {
        NX_INFO(NX_SCOPE_TAG, data);
        const auto result = QJson::deserializeOrThrow<rest::JsonResult>(data);
        EXPECT_EQ(result.error, error) << data.toStdString();
        EXPECT_EQ(result.errorString, errorString) << data.toStdString();
        EXPECT_EQ(result.reply, QJsonValue()) << data.toStdString();
    }

    void jsonOkTest(const QByteArray& data, const QJsonValue& reply)
    {
        NX_INFO(NX_SCOPE_TAG, data);
        const auto result = QJson::deserializeOrThrow<rest::JsonResult>(data);
        EXPECT_EQ(result.error, rest::Result::NoError) << data.toStdString();
        EXPECT_EQ(result.errorString, "") << data.toStdString();
        EXPECT_EQ(result.reply, reply) << data.toStdString();
    }
};

TEST_F(RestResultTest, ResultDeserialize)
{
    baseTest(R"json({"error":"0"})json", rest::Result::NoError);
    baseTest(R"json({"error":"0","errorId":"ok"})json", rest::Result::NoError);
    baseTest(R"json({"error":"0","errorId":"ok","errorString":""})json", rest::Result::NoError);
    baseTest(R"json({"error":"0","errorString":""})json", rest::Result::NoError);

    baseTest(R"json({"error":0})json", rest::Result::NoError);
    baseTest(R"json({"error":0,"errorId":"ok"})json", rest::Result::NoError);
    baseTest(R"json({"error":0,"errorId":"ok","errorString":""})json", rest::Result::NoError);
    baseTest(R"json({"error":0,"errorString":""})json", rest::Result::NoError);

    baseTest(
        R"json({"error":"1","errorId":"missingParameter","errorString":"Error string"})json",
        rest::Result::MissingParameter, "Error string");
    baseTest(
        R"json({"error":"1","errorString":"Error string"})json",
        rest::Result::MissingParameter, "Error string");
    baseTest(
        R"json({"error":"1","errorString":"Error string","reply":null})json",
        rest::Result::MissingParameter, "Error string");

    baseTest(
        R"json({"error":1,"errorId":"missingParameter","errorString":"Error string"})json",
        rest::Result::MissingParameter, "Error string");
    baseTest(
        R"json({"error":1,"errorString":"Error string"})json",
        rest::Result::MissingParameter, "Error string");
    baseTest(
        R"json({"error":1,"errorString":"Error string","reply":null})json",
        rest::Result::MissingParameter, "Error string");
}

TEST_F(RestResultTest, JsonResultDeserialize)
{
    jsonOkTest(R"json({"error":"0","reply":42})json", 42);
    jsonOkTest(R"json({"error":"0","errorId":"ok","reply":"done"})json", "done");
    jsonOkTest(R"json({"error":"0","errorId":"ok","errorString":"","reply":true})json", true);
    jsonOkTest(R"json({"error":"0","errorString":"","reply":[1,2,3]})json", QJsonArray{1, 2, 3});

    jsonErrorTest(
        R"json({"error":"1","errorId":"missingParameter","errorString":"Error string"})json",
        rest::Result::MissingParameter, "Error string");
    jsonErrorTest(
        R"json({"error":"1","errorString":"Error string"})json",
        rest::Result::MissingParameter, "Error string");
    jsonErrorTest(
        R"json({"error":"1","errorString":"Error string","reply":null})json",
        rest::Result::MissingParameter, "Error string");

    jsonErrorTest(
        R"json({"error":1,"errorId":"missingParameter","errorString":"Error string"})json",
        rest::Result::MissingParameter, "Error string");
    jsonErrorTest(
        R"json({"error":1,"errorString":"Error string"})json",
        rest::Result::MissingParameter, "Error string");
    jsonErrorTest(
        R"json({"error":1,"errorString":"Error string","reply":null})json",
        rest::Result::MissingParameter, "Error string");
}

} // namespace nx::network::rest::test
