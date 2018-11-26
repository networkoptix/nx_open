#include <gtest/gtest.h>

#include <rest/server/rest_connection_processor.h>

namespace test {

namespace {

class TestHandler:
    public QnRestRequestHandler
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

using HttpMethod = nx::network::http::Method;

class QnRestProcessorPool:
    public ::testing::Test
{
protected:
    void registerHandler(
        const HttpMethod::ValueType& method,
        const QString& path,
        int handlerId)
    {
        m_pool.registerHandler(method, path, new TestHandler(handlerId));
    }

    void assertHandlerMatched(
        const HttpMethod::ValueType& method,
        const QString& path,
        int handlerId)
    {
        const auto handler = m_pool.findHandler(method, path);
        ASSERT_EQ(handlerId, static_cast<const TestHandler&>(*handler).id());
    }

    void assertHandlerNotMatched(
        const HttpMethod::ValueType& method,
        const QString& path)
    {
        ASSERT_EQ(nullptr, m_pool.findHandler(method, path));
    }

    virtual void SetUp() override
    {
        registerHandler(HttpMethod::get, "res1", 1);
        registerHandler(HttpMethod::post, "res2", 2);
        registerHandler(HttpMethod::delete_, "res3", 3);
        
        registerHandler(::QnRestProcessorPool::kAnyHttpMethod, "res4", 4);
        registerHandler(HttpMethod::options, ::QnRestProcessorPool::kAnyPath, 5);
    }

private:
    ::QnRestProcessorPool m_pool;
};

TEST_F(QnRestProcessorPool, matches_by_method_and_path)
{
    assertHandlerMatched(HttpMethod::post, "res2", 2);
}

TEST_F(QnRestProcessorPool, matches_by_path_only)
{
    assertHandlerMatched(HttpMethod::get, "res4", 4);
}

TEST_F(QnRestProcessorPool, matches_by_method_only)
{
    assertHandlerMatched(HttpMethod::options, "res_unknown", 5);
}

TEST_F(QnRestProcessorPool, unknown_method_is_not_matched)
{
    assertHandlerNotMatched(HttpMethod::put, "res1");
}

TEST_F(QnRestProcessorPool, unknown_path_is_not_matched)
{
    assertHandlerNotMatched(HttpMethod::get, "res2");
}

TEST_F(QnRestProcessorPool, method_has_higher_priority_than_path)
{
    registerHandler(HttpMethod::options, ::QnRestProcessorPool::kAnyPath, 1);
    registerHandler(::QnRestProcessorPool::kAnyHttpMethod, "test", 2);

    assertHandlerMatched(HttpMethod::options, "test", 1);
}

} // namespace test
