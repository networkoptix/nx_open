// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/rest/handler.h>
#include <nx/network/rest/handler_pool.h>

namespace nx::network::rest::test {

namespace {

class TestHandler: public Handler
{
public:
    TestHandler(int id):
        m_id(id)
    {
    }

    int id() const
    {
        return m_id;
    }

private:
    const int m_id;
};

} // namespace

//-------------------------------------------------------------------------------------------------

using GlobalPermission = HandlerPool::GlobalPermission;
using HttpMethod = nx::network::http::Method;

class HandlerPoolTest:
    public ::testing::Test
{
protected:
    void registerHandler(
        const HttpMethod& method,
        const QString& path,
        int handlerId)
    {
        m_pool.registerHandler(method, path, new TestHandler(handlerId));
    }

    void assertHandlerMatched(
        const HttpMethod& method,
        const QString& path,
        int handlerId)
    {
        const auto handler = m_pool.findHandler(method, path);
        ASSERT_NE(nullptr, handler);
        ASSERT_EQ(handlerId, static_cast<const TestHandler*>(handler)->id());
    }

    void assertHandlerNotMatched(
        const HttpMethod& method,
        const QString& path)
    {
        ASSERT_EQ(nullptr, m_pool.findHandler(method, path));
    }

    virtual void SetUp() override
    {
        registerHandler(HttpMethod::get, "res1", 1);
        registerHandler(HttpMethod::post, "res2", 2);
        registerHandler(HttpMethod::delete_, "res3", 3);

        registerHandler(HandlerPool::kAnyHttpMethod, "res4", 4);
        registerHandler(HttpMethod::options, HandlerPool::kAnyPath, 5);
    }

protected:
    HandlerPool m_pool;
};

TEST_F(HandlerPoolTest, matches_by_method_and_path)
{
    assertHandlerMatched(HttpMethod::post, "res2", 2);
}

TEST_F(HandlerPoolTest, matches_by_path_only)
{
    assertHandlerMatched(HttpMethod::get, "res4", 4);
}

TEST_F(HandlerPoolTest, matches_by_method_only)
{
    assertHandlerMatched(HttpMethod::options, "res_unknown", 5);
}

TEST_F(HandlerPoolTest, unknown_method_is_not_matched)
{
    assertHandlerNotMatched(HttpMethod::put, "res1");
}

TEST_F(HandlerPoolTest, unknown_path_is_not_matched)
{
    assertHandlerNotMatched(HttpMethod::get, "res2");
}

TEST_F(HandlerPoolTest, method_has_higher_priority_than_path)
{
    registerHandler(HttpMethod::options, HandlerPool::kAnyPath, 1);
    registerHandler(HandlerPool::kAnyHttpMethod, "test", 2);

    assertHandlerMatched(HttpMethod::options, "test", 1);
}

TEST_F(HandlerPoolTest, path_router)
{
    const auto replace =
        [](std::string_view in, std::string_view what, std::string_view with)
        {
            std::string result(in);
            auto i = in.find(what.data(), 0, what.length());
            for (; i != in.npos; i = in.find(what.data(), i + with.length() + 1, what.length()))
                result.replace(i, what.length(), with.data(), with.length());
            return result;
        };

    const auto nextId =
        []()
        {
            static int firstId = 100;
            return ++firstId;
        };

    const auto assertMatched =
        [this](const std::string& path, int id, std::map<std::string, std::string> params = {})
        {
            nx::network::http::Request httpRequest;
            ASSERT_TRUE(httpRequest.parse(NX_FMT("GET /%1 HTTP/1.1", path).toStdString()));
            Request request(&httpRequest, kSystemSession);
            auto handler = dynamic_cast<TestHandler*>(m_pool.findHandlerOrThrow(&request));
            ASSERT_TRUE(handler) << request.decodedPath().toStdString();
            ASSERT_EQ(handler->id(), id) << request.decodedPath().toStdString();
            for (const auto& [key, value]: params)
            {
                ASSERT_EQ(request.params()[QString::fromStdString(key)].toStdString(), value)
                    << request.decodedPath().toStdString() << " " << key << ": " << value;
            }
        };

    const auto assertNotMatched =
        [this](const std::string& path)
        {
            nx::network::http::Request httpRequest;
            ASSERT_TRUE(httpRequest.parse(NX_FMT("GET /%1 HTTP/1.1", path).toStdString()));
            Request request(&httpRequest, kSystemSession);
            ASSERT_FALSE(m_pool.findHandlerOrThrow(&request))
                << request.decodedPath().toStdString();
        };

    const int kFixedId = nextId();
    const char* kFixedPath = "fixed/tail";
    m_pool.registerHandler(
        kFixedPath, GlobalPermission::none, std::make_unique<TestHandler>(kFixedId));

    const int kRequiredId1 = nextId();
    const char* kRequiredPath1 = "required/:key";
    m_pool.registerHandler(
        kRequiredPath1, GlobalPermission::none, std::make_unique<TestHandler>(kRequiredId1));

    const int kRequiredId2 = nextId();
    const char* kRequiredPath2 = "required/:key/tail";
    m_pool.registerHandler(
        kRequiredPath2, GlobalPermission::none, std::make_unique<TestHandler>(kRequiredId2));

    const int kRequiredId3 = nextId();
    const char* kRequiredPath3 = "required3/:key/tail";
    m_pool.registerHandler(
        kRequiredPath3, GlobalPermission::none, std::make_unique<TestHandler>(kRequiredId3));

    const int kOptionalId1 = nextId();
    const char* kOptionalPath1 = "optional/:key?";
    m_pool.registerHandler(
        kOptionalPath1, GlobalPermission::none, std::make_unique<TestHandler>(kOptionalId1));

    const int kOptionalId2 = nextId();
    const char* kOptionalPath2 = "optional/:key?/tail";
    m_pool.registerHandler(
        kOptionalPath2, GlobalPermission::none, std::make_unique<TestHandler>(kOptionalId2));

    const int kOptionalId3 = nextId();
    const char* kOptionalPath3 = "optional3/:key?/tail";
    m_pool.registerHandler(
        kOptionalPath3, GlobalPermission::none, std::make_unique<TestHandler>(kOptionalId3));

    const int kTailId = nextId();
    const char* kTailPath = "tail/:key+";
    m_pool.registerHandler(
        kTailPath, GlobalPermission::none, std::make_unique<TestHandler>(kTailId));

    assertMatched(kFixedPath, kFixedId);
    assertNotMatched(replace(kFixedPath, "/tail", ""));
    assertNotMatched(replace(kFixedPath, "tail", "invalid"));
    assertNotMatched(std::string(kFixedPath) + "/after");

    assertMatched(replace(kRequiredPath1, ":key", "value"), kRequiredId1, {{"key", "value"}});
    assertNotMatched(replace(kRequiredPath1, ":key", "value") + "/after");

    assertMatched(replace(kRequiredPath2, ":key", "value"), kRequiredId2, {{"key", "value"}});
    assertNotMatched(replace(replace(kRequiredPath2, ":key", "value"), "tail", "invalid"));
    assertNotMatched(replace(replace(kRequiredPath2, "/:key", ""), "/tail", ""));
    assertNotMatched(replace(kRequiredPath2, ":key", "value") + "/after");

    assertMatched(replace(kRequiredPath3, ":key", "value"), kRequiredId3, {{"key", "value"}});
    assertNotMatched(replace(replace(kRequiredPath3, ":key", "value"), "tail", ""));
    assertNotMatched(replace(replace(kRequiredPath3, ":key", "value"), "tail", "invalid"));
    assertNotMatched(replace(replace(kRequiredPath3, "/:key", ""), "/tail", ""));
    assertNotMatched(replace(kRequiredPath3, ":key", "value") + "/after");

    assertMatched(replace(kOptionalPath1, ":key?", "value"), kOptionalId1, {{"key", "value"}});
    assertMatched(replace(kOptionalPath1, ":key?", ""), kOptionalId1, {{"key", ""}});
    assertNotMatched(replace(kOptionalPath1, ":key?", "value") + "/after");

    assertMatched(replace(kOptionalPath2, ":key?", "value"), kOptionalId2, {{"key", "value"}});
    assertMatched(replace(kOptionalPath2, ":key?", ""), kOptionalId2, {{"key", ""}});
    assertNotMatched(replace(replace(kOptionalPath2, ":key?", "value"), "tail", "invalid"));
    assertNotMatched(replace(kOptionalPath2, ":key?", "value") + "/after");

    assertMatched(replace(kOptionalPath3, ":key?", "value"), kOptionalId3, {{"key", "value"}});
    assertMatched(replace(kOptionalPath3, ":key?", ""), kOptionalId3, {{"key", ""}});
    assertNotMatched(replace(replace(kOptionalPath3, "/:key?", "value"), "/tail", ""));
    assertNotMatched(replace(replace(kOptionalPath3, "/:key?", ""), "/tail", ""));
    assertNotMatched(replace(replace(kOptionalPath3, ":key?", "value"), "tail", "invalid"));
    assertNotMatched(replace(kOptionalPath3, ":key?", "value") + "/after");

    assertMatched(replace(kTailPath, ":key+", "tail/value"), kTailId, {{"key", "tail/value"}});
}

} // namespace nx::network::rest::test
