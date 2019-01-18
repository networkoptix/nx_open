#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/maintenance/log/client.h>
#include <nx/network/maintenance/log/request_path.h>
#include <nx/network/maintenance/log/server.h>
#include <nx/network/url/url_builder.h>

#include <nx/utils/log/logger_collection.h>
#include <nx/utils/log/log_level.h>

namespace nx::network::maintenance::log::test {

using namespace nx::utils::log;

static constexpr char kBasePath[] = "/log";
static constexpr const char* kTags[] =
{
    "nx::network",
    "nx::utils::",
    "nx::utils::log",
    "nx::network::maintenance",
    "nx::network::maintenance::log",
    "nx::network::http",
    "nx::network::maintenance::log::test",
    nullptr
};

class LogServer:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());

        m_httpClient.setResponseReadTimeout(kNoTimeout);
        m_httpClient.setMessageBodyReadTimeout(kNoTimeout);

        m_loggerCollection = std::make_unique<nx::utils::log::LoggerCollection>();
        m_logServer = std::make_unique<maintenance::log::Server>(m_loggerCollection.get());

        m_logServer->registerRequestHandlers(
            kBasePath,
            &m_httpServer.httpMessageDispatcher());
    }

    void whenRequestLogConfiguration()
    {
        ASSERT_TRUE(m_httpClient.doGet(createRequestUrl(kLoggers)));
    }

    void thenGetRequestSucceeded()
    {
        ASSERT_EQ(http::StatusCode::ok, m_httpClient.response()->statusLine.statusCode);
    }

    void andActualConfigurationIsProvided()
    {
        const auto messageBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(messageBody);

        bool parseSucceeded = false;
        auto loggers = QJson::deserialized<Loggers>(*messageBody, {}, &parseSucceeded);
        ASSERT_TRUE(parseSucceeded);

        ASSERT_EQ(loggers.loggers.size(), 2);
        
        ASSERT_NE(loggers.loggers[0].id, LoggerCollection::invalidId);
        ASSERT_FALSE(loggers.loggers[0].path.empty());
        ASSERT_FALSE(loggers.loggers[0].filters.empty());

        ASSERT_NE(loggers.loggers[1].id, LoggerCollection::invalidId);
        ASSERT_TRUE(loggers.loggers[1].path.empty()); //< should be stdout logger, no filename.
        ASSERT_FALSE(loggers.loggers[1].filters.empty());
    }

    void andAddLoggerRequestFailed()
    {
        ASSERT_EQ(http::StatusCode::badRequest, m_httpClient.response()->statusLine.statusCode);
    }

    void whenAddLoggerConfiguration()
    {
        Filter f;
        f.level = "verbose";
        f.tags = { kTags[0] };
        
        Filter f1;
        f1.level = "info";
        f.tags = { kTags[1], kTags[2] };

        Logger loggerInfo;
        loggerInfo.filters = { f, f1 };
        loggerInfo.path = "C:/develop/file_log";

        ASSERT_TRUE(m_httpClient.doPost(
            createRequestUrl(kLoggers),
            kApplicationType,
            QJson::serialized(loggerInfo)));
    }

    void whenAddStdOutConfigurationWithDuplicateTag()
    {
        Filter f;
        f.level = "always";
        f.tags = { kTags[0], kTags[3] }; //<kTags[0] is duplicate with first logger

        Filter f1;
        f1.level = "debug";
        f1.tags = { kTags[4], kTags[5] };

        Logger loggerInfo;
        loggerInfo.filters = { f, f1 };
        loggerInfo.path = "-"; // <stdout logger

        ASSERT_TRUE(m_httpClient.doPost(
            createRequestUrl(kLoggers),
            kApplicationType,
            QJson::serialized(loggerInfo)));
    }

    void thenPostRequestSucceeded()
    {
        ASSERT_EQ(http::StatusCode::created, m_httpClient.response()->statusLine.statusCode);
    }

    void thenPostRequestFailed()
    {
        ASSERT_EQ(http::StatusCode::badRequest, m_httpClient.response()->statusLine.statusCode);
    }

    void andNewLoggerConfigurationIsProvided()
    {
        const auto messageBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(messageBody);

        bool parseSucceeded = false;
        auto loggerInfo = QJson::deserialized<Logger>(*messageBody, {}, &parseSucceeded);
        ASSERT_TRUE(parseSucceeded);
    }

    void whenDeleteExistingLogger()
    {
        ASSERT_TRUE(m_httpClient.doDelete(createDeleteRequestUrl(kLoggers, 0)));
    }

    void thenDeleteRequestSucceeded()
    {
        ASSERT_EQ(http::StatusCode::ok, m_httpClient.response()->statusLine.statusCode);
    }

    void whenDeleteNonExistingLogger()
    {
        ASSERT_TRUE(m_httpClient.doDelete(createDeleteRequestUrl(kLoggers, 2)));
    }

    void thenDeleteRequestFailed()
    {
        ASSERT_EQ(http::StatusCode::notFound, m_httpClient.response()->statusLine.statusCode);
    }

    void addTwoLoggersOrFail()
    {
        // Logger 1.
        whenAddLoggerConfiguration();

        thenPostRequestSucceeded();
        andNewLoggerConfigurationIsProvided();
        
        // Logger 2.
        whenAddStdOutConfigurationWithDuplicateTag();

        thenPostRequestSucceeded();
        andNewLoggerConfigurationIsProvided();
    }

private:
    http::TestHttpServer m_httpServer;
    std::unique_ptr<nx::utils::log::LoggerCollection> m_loggerCollection = nullptr;
    std::unique_ptr<maintenance::log::Server> m_logServer;
    http::HttpClient m_httpClient;

    nx::utils::Url createRequestUrl(const std::string& requestName)
    {
        return url::Builder().setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress())
            .setPath(kBasePath).appendPath(requestName);
    }

    nx::utils::Url createDeleteRequestUrl(const std::string& requestName, int loggerId)
    {
        return url::Builder().setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress())
            .setPath(kBasePath).appendPath(requestName)                                  
            .appendPath("/").appendPath(std::to_string(loggerId));
    }
};

TEST_F(LogServer, DISABLED_log_configuration_is_provided)
{
    addTwoLoggersOrFail();

    whenRequestLogConfiguration();

    thenGetRequestSucceeded();
    andActualConfigurationIsProvided();//< Expects two loggers with specific properties.
}

TEST_F(LogServer, DISABLED_add_log_configuration)
{
    addTwoLoggersOrFail();
}


TEST_F(LogServer, DISABLED_server_fails_to_add_logger_when_all_tags_are_duplicates)
{
    // Add a logging configuration
    whenAddLoggerConfiguration();

    thenPostRequestSucceeded();
    andNewLoggerConfigurationIsProvided();

    // Add the same logging configuration a second time.
    whenAddLoggerConfiguration();
    thenPostRequestFailed();
}

TEST_F(LogServer, DISABLED_server_deletes_existing_log_configuration)
{
    // Setup the logging collection first.
    addTwoLoggersOrFail();
    
    whenDeleteExistingLogger();
    thenDeleteRequestSucceeded();
}                                    

TEST_F(LogServer, DISABLED_server_fails_to_delete_non_existing_logging_configuration)
{
    // Setup the logging collection first.
    addTwoLoggersOrFail();

    whenDeleteNonExistingLogger();
    thenDeleteRequestFailed();
}

} // namespace nx::network::maintenance::log::test
