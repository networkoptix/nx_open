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

namespace {

static constexpr char kBasePath[] = "/log";

} // namespace 

class LogServer:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());

        m_httpClient.setResponseReadTimeout(kNoTimeout);
        m_httpClient.setMessageBodyReadTimeout(kNoTimeout);
        
        m_logServer = std::make_unique<maintenance::log::Server>(loggerCollection());

        m_logServer->registerRequestHandlers(
            kBasePath,
            &m_httpServer.httpMessageDispatcher());
    }

    void whenRequestLogConfiguration()
    {
        ASSERT_TRUE(m_httpClient.doGet(createRequestUrl(kLoggers)));
    }

    void andActualConfigurationIsProvided(const std::vector<Logger>& loggers)
    {
        const auto messageBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(messageBody);

        bool parseSucceeded = false;
        Loggers loggersReceived = QJson::deserialized<Loggers>(*messageBody, {}, &parseSucceeded);
        ASSERT_TRUE(parseSucceeded);

        ASSERT_EQ(loggersReceived.loggers, loggers);
    }

    void andAddLoggerRequestFailed()
    {
        ASSERT_EQ(http::StatusCode::badRequest, m_httpClient.response()->statusLine.statusCode);
    }

    void whenAddLoggerConfiguration(const std::string& tag, Logger* outLogger = nullptr)
    {
        Filter f;
        f.level = "verbose";
        f.tags = { tag };
        
        Filter f1;
        f1.level = "info";
        f1.tags = { "nx::utils", "nx::utils::log" };

        Logger logger;
        logger.filters = { f, f1 };
        logger.path = "C:/develop/some_log_file";

        ASSERT_TRUE(m_httpClient.doPost(
            createRequestUrl(kLoggers),
            "application/json",
            QJson::serialized(logger)));

        if (outLogger)
            *outLogger = logger;
    }

    void whenAddStdOutConfigurationWithDuplicateTag(
        const std::string& tag,
        Logger* outLogger = nullptr)
    {
        Filter f;
        f.level = "always";
        f.tags = { tag, "nx::network::maintenance" };

        Filter f1;
        f1.level = "debug";
        f1.tags = { "nx::network::maintenance::log", "nx::network::http" };

        Logger logger;
        logger.filters = { f, f1 };
        logger.path = "-"; // <stdout logger

        ASSERT_TRUE(m_httpClient.doPost(
            createRequestUrl(kLoggers),
            "application/json",
            QJson::serialized(logger)));

        if (outLogger)
            *outLogger = logger;
    }

    void andNewLoggerConfigurationIsProvided(Logger * outNewLoggerInfo = nullptr)
    {
        const auto messageBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(messageBody);

        bool parseSucceeded = false;
        auto newLoggerInfo = QJson::deserialized<Logger>(*messageBody, {}, &parseSucceeded);
        ASSERT_TRUE(parseSucceeded);

        if (outNewLoggerInfo)
            *outNewLoggerInfo = newLoggerInfo;
    }

    void assertLoggerHasTag(const Logger& logger, const std::string& tag)
    {
        bool hasTag = false;
        for (const auto& filter : logger.filters)
        {
            if (std::find(filter.tags.begin(), filter.tags.end(), tag) != filter.tags.end())
                hasTag = true;
        }

        ASSERT_TRUE(hasTag);
    }

    void assertLoggerDoesNotHaveTag(const Logger& logger, const std::string& tag)
    {
        bool hasTag = false;
        for (const auto& filter : logger.filters)
        {
            if (std::find(filter.tags.begin(), filter.tags.end(), tag) != filter.tags.end())
                hasTag = true;
        }

        ASSERT_FALSE(hasTag);
    }

    void whenDeleteLoggerConfiguration(int loggerId)
    {
        ASSERT_TRUE(m_httpClient.doDelete(createDeleteRequestUrl(kLoggers, loggerId)));
    }

    void thenRequestSucceeded(const http::StatusCode::Value& expectedCode)
    {
        ASSERT_EQ(expectedCode, m_httpClient.response()->statusLine.statusCode);
    }

    void thenRequestFailed(const http::StatusCode::Value& expectedCode)
    {
        ASSERT_EQ(expectedCode, m_httpClient.response()->statusLine.statusCode);
    }

    void givenTwoLoggers(std::vector<Logger>* outloggers = nullptr)
    {
        Logger loggerInfo;
        std::string tag("nx::network");

        // Logger 1.
        whenAddLoggerConfiguration(tag);

        thenRequestSucceeded(http::StatusCode::created);
        andNewLoggerConfigurationIsProvided(&loggerInfo);
        
        if (outloggers)
            outloggers->push_back(loggerInfo);

        // Logger 2.
        whenAddStdOutConfigurationWithDuplicateTag(tag);

        thenRequestSucceeded(http::StatusCode::created);
        andNewLoggerConfigurationIsProvided(&loggerInfo);
        
        if (outloggers)
            outloggers->push_back(loggerInfo);
    }

    void whenAddLoggerStreamingConfiguration(Level level, const std::string& tag)
    {
        std::string levelStr = toString(level).toStdString();
        std::string query = std::string("level=" + levelStr + "[") + tag.c_str() + "],level=none";
        ASSERT_TRUE(m_httpClient.doGet(createRequestUrl(kStream, query)));
    }

    void thenLogsAreProduced(
        const Level level,
        const std::string& tagName,
        int count,
        const std::string& logThisString = std::string())
    {
        Tag tag(tagName);

        for (int i = 0; i < count; ++i)
            loggerCollection()->get(tag, false)->log(level, tag, QString::number(i) + '\n');

        loggerCollection()->get(tag, false)->log(level, tag, QString(logThisString.c_str()) + '\n');
    }

    void andLogStreamIsProvided(const std::string& stringToFind)
    {
        QString s;
        while (!s.contains(stringToFind.c_str()))
            s += m_httpClient.fetchMessageBodyBuffer();
    }

    void andLoggerIsRemoved(int loggerId)
    {
        loggerCollection()->remove(loggerId);
    }

    void whenAddLoggingConfigurationWithMalformedJson()
    {
        ASSERT_TRUE(m_httpClient.doPost(
            createRequestUrl(kLoggers),
            "application/json",
            getMalformedJson().toUtf8()));
    }

    void whenRequestLogStreamWithMalformeQueryString()
    {
        std::string malformedJson = "level=verbnx::network],level=none"; //< "verb" instead of "verbose"
        ASSERT_TRUE(m_httpClient.doGet(createRequestUrl(kStream, malformedJson)));
    }

    LoggerCollection* loggerCollection()
    {
        return &m_loggerCollection;
    }

    static QString getMalformedJson()
    {
        return
            QString("{")
            + "\"file\": \"/path/to/log/file\","
            + "\"filters\": ["
            + "{\"level: \"verbose\", \"tags\": [\"nx::network\", \"nx::utils\"]}," //< Missing \" after level
            + "{ \"level\": \"none\"}"
            + "]"
            + ""; //< Should be "}"
    }

private:
    LoggerCollection m_loggerCollection;
    http::TestHttpServer m_httpServer;
    http::HttpClient m_httpClient;
    std::unique_ptr<Server> m_logServer;

    nx::utils::Url createRequestUrl(
        const std::string& requestName,
        const std::string& query = std::string())
    {
        url::Builder builder;
        builder.setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress())
            .setPath(kBasePath).appendPath(requestName);
        if (!query.empty())
            builder.setQuery(query.c_str());
        return builder;
    }

    nx::utils::Url createDeleteRequestUrl(const std::string& requestName, int loggerId)
    {
        return url::Builder().setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress())
            .setPath(kBasePath).appendPath(requestName)                                  
            .appendPath("/").appendPath(std::to_string(loggerId));
    }
};

TEST_F(LogServer, server_provides_all_loggers)
{
    std::vector<Logger> loggers;
    givenTwoLoggers(&loggers);

    whenRequestLogConfiguration();

    thenRequestSucceeded(http::StatusCode::ok);
    andActualConfigurationIsProvided(loggers);
}

TEST_F(LogServer, server_accepts_new_logger)
{
    whenAddLoggerConfiguration("nx::network");

    thenRequestSucceeded(http::StatusCode::created);
    andNewLoggerConfigurationIsProvided();
}

TEST_F(
    LogServer,
    server_accepts_two_loggers_with_some_duplicate_tags_and_second_logger_does_not_have_duplicates)
{
    std::string tag("nx::network");

    Logger loggerInfo1;
    whenAddLoggerConfiguration(tag);

    thenRequestSucceeded(http::StatusCode::created);
    andNewLoggerConfigurationIsProvided(&loggerInfo1);

    Logger loggerInfo2;
    whenAddStdOutConfigurationWithDuplicateTag(tag);

    thenRequestSucceeded(http::StatusCode::created);
    andNewLoggerConfigurationIsProvided(&loggerInfo2);

    assertLoggerHasTag(loggerInfo1, tag);
    assertLoggerDoesNotHaveTag(loggerInfo2, tag);
}

TEST_F(LogServer, server_rejects_logger_when_all_tags_are_duplicates)
{
    std::string tag("nx::network");
    
    // Add a logger.
    whenAddLoggerConfiguration(tag);

    thenRequestSucceeded(http::StatusCode::created);
    andNewLoggerConfigurationIsProvided();

    // Add the same logger a second time.
    whenAddLoggerConfiguration(tag);
    thenRequestFailed(http::StatusCode::badRequest);
}

TEST_F(LogServer, server_deletes_existing_logger_configuration)
{
    std::vector<Logger> loggers;
    givenTwoLoggers(&loggers);
    
    whenDeleteLoggerConfiguration(loggers[0].id);
    thenRequestSucceeded(http::StatusCode::ok);
}                                    

TEST_F(LogServer, server_fails_to_delete_non_existing_logger_configuration)
{
    givenTwoLoggers();

    whenDeleteLoggerConfiguration(2 /*loggerId*/);
    thenRequestFailed(http::StatusCode::notFound);
}

TEST_F(LogServer, server_streams_logs)
{
    Level level(Level::debug);
    std::string tag("nx::network");
    std::string targetString("find me");
    int logsToProduce = 20;

    whenAddLoggerStreamingConfiguration(level, tag);

    thenRequestSucceeded(http::StatusCode::created);
    thenLogsAreProduced(level, tag, logsToProduce, targetString);
    andLogStreamIsProvided(targetString);
}

TEST_F(LogServer, server_rejects_malformated_json_during_post_logger)
{
    whenAddLoggingConfigurationWithMalformedJson();
    
    thenRequestFailed(http::StatusCode::badRequest);
}

TEST_F(LogServer, server_rejects_log_stream_request_with_malformed_query_string)
{
    whenRequestLogStreamWithMalformeQueryString();

    thenRequestFailed(http::StatusCode::badRequest);
}

TEST_F(LogServer, DISABLED_server_crashes_when_logger_is_removed_during_stream)
{
    Level level(Level::debug);
    std::string tag("nx::network");

    whenAddLoggerStreamingConfiguration(level, tag);
    
    thenRequestSucceeded(http::StatusCode::created);
    Logger newLogger;
    andNewLoggerConfigurationIsProvided(&newLogger);

    std::thread thread([&]()
    {
        thenLogsAreProduced(level, tag, 100);
    });

    andLoggerIsRemoved(newLogger.id); //<should crash

    thread.join();
}

} // namespace nx::network::maintenance::log::test
