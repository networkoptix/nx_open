// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/maintenance/log/client.h>
#include <nx/network/maintenance/log/utils.h>
#include <nx/network/maintenance/log/server.h>
#include <nx/network/maintenance/log/request_path.h>
#include <nx/reflect/json.h>

#include <nx/utils/log/logger_collection.h>
#include <nx/utils/log/log_level.h>
#include <nx/utils/string.h>

namespace nx::network::maintenance::log::test {

using namespace nx::log;

namespace {

static constexpr char kBasePath[] = "/log";
static constexpr Level kLevel = Level::debug;
static constexpr char kTag[] = "nx::network";
static constexpr char kTargetString[] = "find me";

} // namespace

class LogServer:
    public ::testing::Test
{
public:
    ~LogServer()
    {
        m_httpClient.reset(nullptr);
        m_httpServer.reset(nullptr);
        m_logServer.reset(nullptr);
    }

protected:
    virtual void SetUp() override
    {
        m_httpServer = std::make_unique<http::TestHttpServer>();
        ASSERT_TRUE(m_httpServer->bindAndListen());

        m_httpClient = std::make_unique<http::HttpClient>(ssl::kAcceptAnyCertificate);
        m_httpClient->setResponseReadTimeout(kNoTimeout);
        m_httpClient->setMessageBodyReadTimeout(kNoTimeout);

        m_logServer = std::make_unique<maintenance::log::Server>(loggerCollection());

        m_logServer->registerRequestHandlers(
            kBasePath,
            &m_httpServer->httpMessageDispatcher());
    }

    void givenExistingLoggerConfiguration()
    {
        whenAddLoggerConfiguration();
        thenRequestSucceeded(http::StatusCode::created);
        andNewLoggerConfigurationIsProvided(/*compareFilters*/ true);
    }

    void givenTwoLoggers()
    {
        givenExistingLoggerConfiguration();

        // Logger 2.
        addLogger(getDefaultLogger2());
        thenRequestSucceeded(http::StatusCode::created);
        andNewLoggerConfigurationIsProvided(false);
    }

    void givenStreamingLogger()
    {
        whenAddLoggerStreamingConfiguration();
        thenRequestSucceeded(http::StatusCode::ok);
    }

    void whenRequestLogConfiguration()
    {
        ASSERT_TRUE(m_httpClient->doGet(createRequestUrl(kLoggers)));
    }

    void whenAddLoggerConfiguration()
    {
        m_duplicateTag = "nx::network";
        addLogger(getDefaultLogger(m_duplicateTag));
    }

    void whenAddLoggerConfigurationWithAllDuplicateTags()
    {
        whenAddLoggerConfiguration();
    }

    void whenAddLoggerConfigurationWithDuplicateTag()
    {
        addLogger(getDefaultLogger2(m_duplicateTag));
    }

    void whenLoggerIsRemoved()
    {
        loggerCollection()->remove(0/*loggerId*/);
    }

    void whenAddLoggingConfigurationWithMalformedJson()
    {
        ASSERT_TRUE(m_httpClient->doPost(
            createRequestUrl(kLoggers),
            "application/json",
            nx::Buffer(getMalformedJson())));
    }

    void whenAddLoggerStreamingConfiguration()
    {
        std::string query("level=debug[nx::network],level=none");
        ASSERT_TRUE(m_httpClient->doGet(createRequestUrl(kStream, query)));
    }

    void whenServerConnectionIsClosed()
    {
        m_httpClient.reset(nullptr);
    }

    void thenLoggerIsRemovedByServer()
    {
        while (loggerCollection()->get(0))
        {
            static const std::chrono::milliseconds kSleep(100);
            std::this_thread::sleep_for(kSleep);
        }
        ASSERT_EQ(loggerCollection()->get(0), nullptr);
    }

    void whenRequestLogStreamWithMalformedQueryString()
    {
        std::string malformedJson = "level=verbnx::network],level=none"; //< "verb" instead of "verbose"
        ASSERT_TRUE(m_httpClient->doGet(createRequestUrl(kStream, malformedJson)));
    }

    void whenDeleteLoggerConfiguration(int loggerId = -1)
    {
        if (loggerId == -1)
            loggerId = m_loggersSavedFromPost.back().id;
        ASSERT_TRUE(m_httpClient->doDelete(createDeleteRequestUrl(kLoggers, loggerId)));
    }

    void whenDeleteLoggerUsingUnknownId()
    {
        // expected logger ids are 0 and 1
        whenDeleteLoggerConfiguration(2);
    }

    void thenRequestSucceeded(const http::StatusCode::Value& expectedCode)
    {
        ASSERT_EQ(expectedCode, m_httpClient->response()->statusLine.statusCode);
    }

    void thenRequestFailed(const http::StatusCode::Value& expectedCode)
    {
        ASSERT_EQ(expectedCode, m_httpClient->response()->statusLine.statusCode);
    }

    void thenLogsAreProduced()
    {
        std::string tagStr(kTag);
        Tag tag(tagStr);

        for (int i = 0; i < 20; ++i)
        {
            if (auto logger = loggerCollection()->get(tag, false))
                logger->log(kLevel, tag, (std::to_string(i) + '\n').c_str());
        }

        if (auto logger = loggerCollection()->get(tag, false))
            logger->log(kLevel, tag, (std::string(kTargetString) + '\n').c_str());
    }

    void andActualConfigurationIsProvided()
    {
        const auto messageBody = m_httpClient->fetchEntireMessageBody();
        ASSERT_TRUE(messageBody);

        const auto [loggersReturnedByServer, result] =
            nx::reflect::json::deserialize<LoggerList>(*messageBody);
        ASSERT_TRUE(result.success);

        // NOTE: size can't be compared because the logging server may have its main logger set.
        // If so, then there will be one extra logger returned by the GET loggers request
        // than in the list of loggers saved from POST requests to the server.

        const auto serverHasLogger =
            [this, loggersReturnedByServer = loggersReturnedByServer](const Logger& logger) -> bool
            {
                auto it = std::find_if(
                    loggersReturnedByServer.begin(),
                    loggersReturnedByServer.end(),
                    [this, &logger](const Logger& a) { return loggersAreEqual(a, logger); });
                return it != loggersReturnedByServer.end();
            };

        for (std::size_t i = 0; i < m_loggersSavedFromPost.size(); ++i)
        {
            ASSERT_TRUE(serverHasLogger(m_loggersSavedFromPost[i]));
        }
    }

    void andFirstLoggerHasTag()
    {
        ASSERT_TRUE(loggerHasTag(m_loggersSavedFromPost[0], m_duplicateTag));
    }

    void andSecondLoggerDoesNotHaveTag()
    {
        ASSERT_FALSE(loggerHasTag(m_loggersSavedFromPost[1], m_duplicateTag));
    }

    void andAddloggerToAddFailed()
    {
        ASSERT_EQ(http::StatusCode::badRequest, m_httpClient->response()->statusLine.statusCode);
    }

    void andNewLoggerConfigurationIsProvided(bool compareFilters = true)
    {
        const auto messageBody = m_httpClient->fetchEntireMessageBody();
        ASSERT_TRUE(messageBody);

        const auto [loggerReturnedByServer, result] =
            nx::reflect::json::deserialize<Logger>(*messageBody);
        ASSERT_TRUE(result.success);

        const Logger& expected = m_expectedLoggers.back();

        ASSERT_TRUE(defaultLevelsAreEqual(expected, loggerReturnedByServer));
        if (compareFilters)
        {
            ASSERT_TRUE(filtersAreEqual(expected, loggerReturnedByServer));
        }

        m_loggersSavedFromPost.emplace_back(std::move(loggerReturnedByServer));
    }

    void andLogStreamIsFetched()
    {
        std::string s;
        while (!nx::utils::contains(s, kTargetString))
            s += m_httpClient->fetchMessageBodyBuffer();
    }

private:
    LoggerCollection* loggerCollection()
    {
        return &m_loggerCollection;
    }

    Logger getDefaultLogger(
        const std::string& tag = "nx::network",
        const std::string& filePath = "C:/develop/some_log_file")
    {
        Filter f;
        f.level = "verbose";
        f.tags = { tag };

        Filter f1;
        f1.level = "info";
        f1.tags = { "nx::utils", "nx::log" };

        Logger logger;
        logger.filters = { f, f1 };
        logger.path = filePath;

        return logger;
    }

    Logger getDefaultLogger2(
        const std::string& tag = "nx::network",
        const std::string& filePath = "-"/*stdout*/)
    {
        Filter f;
        f.level = "always";
        f.tags = { tag, "nx::network::maintenance" };

        Filter f1;
        f1.level = "debug";
        f1.tags = { "nx::network::maintenance::log", "nx::network::http" };

        Logger logger;
        logger.filters = { f, f1 };
        logger.path = filePath;

        return logger;
    }

    static std::string getMalformedJson()
    {
        return std::string("{")
            + "\"file : \"/path/to/log/file\"," //< Missing \" after file
            + "\"filters\": ["
            + "{\"level\": \"verbose\", \"tags\": [\"nx::network\", \"nx::utils\"]},"
            + "{ \"level\": \"none\"}"
            + "]"
            + ""; //< Should be "}"
    }

    bool loggersAreEqual(const Logger& a, const Logger& b)
    {
        if (a.id != b.id)
            return false;
        if (a.path != b.path)
            return false;
        if (!defaultLevelsAreEqual(a, b))
            return false;
        return true;
    }

    bool defaultLevelsAreEqual(const Logger& a, const Logger& b)
    {
        if (a.defaultLevel == "none" && (b.defaultLevel == "none" || b.defaultLevel.empty()))
            return true;
        if (b.defaultLevel == "none" && (a.defaultLevel == "none" || a.defaultLevel.empty()))
            return true;
        return a.defaultLevel == b.defaultLevel;
    }

    bool filtersAreEqual(const Logger& a, const Logger& b)
    {
        LevelFilters aFilters = utils::toLevelFilters(a.filters);
        LevelFilters bFilters = utils::toLevelFilters(b.filters);
        if (aFilters.size() != bFilters.size())
            return false;
        for (const auto& element : aFilters)
        {
            auto it = bFilters.find(element.first);
            if (it == bFilters.end())
                return false;
            if (it->first.toString() != element.first.toString())
                return false;
            if (it->second != element.second)
                return false;
        }
        return true;
    }

    bool loggerHasTag(const Logger& logger, const std::string& tag)
    {
        for (const auto& filter : logger.filters)
        {
            if (std::find(filter.tags.begin(), filter.tags.end(), tag) != filter.tags.end())
                return true;
        }
        return false;
    }

    void addLogger(const Logger& logger)
    {
        ASSERT_TRUE(m_httpClient->doPost(
            createRequestUrl(kLoggers),
            "application/json",
            nx::Buffer(nx::reflect::json::serialize(logger))));
        m_expectedLoggers.emplace_back(logger);
    }

    nx::utils::Url createRequestUrl(
        const std::string& requestName,
        const std::string& query = std::string())
    {
        url::Builder builder;
        builder.setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer->serverAddress())
            .setPath(kBasePath).appendPath(requestName);
        if (!query.empty())
            builder.setQuery(query);
        return builder;
    }

    nx::utils::Url createDeleteRequestUrl(const std::string& requestName, int loggerId)
    {
        return url::Builder().setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer->serverAddress())
            .setPath(kBasePath).appendPath(requestName)
            .appendPath("/").appendPath(std::to_string(loggerId));
    }

    private:
        LoggerCollection m_loggerCollection;
        std::unique_ptr<http::TestHttpServer> m_httpServer;
        std::unique_ptr<http::HttpClient> m_httpClient;
        std::unique_ptr<maintenance::log::Server> m_logServer;
        std::vector<Logger> m_expectedLoggers;
        std::vector<Logger> m_loggersSavedFromPost;
        std::string m_duplicateTag;
};

TEST_F(LogServer, accepts_new_logger)
{
    whenAddLoggerConfiguration();

    thenRequestSucceeded(http::StatusCode::created);
    andNewLoggerConfigurationIsProvided();
}

TEST_F(
    LogServer,
    accepts_loggers_with_a_duplicate_tag_and_second_logger_does_not_have_duplicate)
{
    givenExistingLoggerConfiguration();

    whenAddLoggerConfigurationWithDuplicateTag();

    thenRequestSucceeded(http::StatusCode::created);

    andNewLoggerConfigurationIsProvided(/*compareFilters*/ false);
    andFirstLoggerHasTag();
    andSecondLoggerDoesNotHaveTag();
}

TEST_F(LogServer, provides_all_logger_configurations)
{
    givenTwoLoggers();

    whenRequestLogConfiguration();

    thenRequestSucceeded(http::StatusCode::ok);
    andActualConfigurationIsProvided();
}

TEST_F(LogServer, rejects_logger_configuration_with_duplicate_tags)
{
    givenExistingLoggerConfiguration();

    whenAddLoggerConfigurationWithAllDuplicateTags();
    thenRequestFailed(http::StatusCode::badRequest);
}

TEST_F(LogServer, deletes_existing_logger_configuration)
{
    givenExistingLoggerConfiguration();

    whenDeleteLoggerConfiguration();
    thenRequestSucceeded(http::StatusCode::ok);
}

TEST_F(LogServer, fails_to_delete_non_existing_logger_configuration)
{
    givenTwoLoggers();

    whenDeleteLoggerUsingUnknownId();
    thenRequestFailed(http::StatusCode::notFound);
}

TEST_F(LogServer, streams_logs_by_adding_custom_logging_configuration)
{
    whenAddLoggerStreamingConfiguration();

    thenRequestSucceeded(http::StatusCode::ok);
    thenLogsAreProduced();
    andLogStreamIsFetched();
}

TEST_F(LogServer, rejects_malformed_json_when_adding_logger_configuration)
{
    whenAddLoggingConfigurationWithMalformedJson();

    thenRequestFailed(http::StatusCode::badRequest);
}

TEST_F(LogServer, rejects_logger_stream_request_with_malformed_query_string)
{
    whenRequestLogStreamWithMalformedQueryString();

    thenRequestFailed(http::StatusCode::badRequest);
}

TEST_F(LogServer, connection_closure_causes_server_to_remove_logger)
{
    givenStreamingLogger();

    whenServerConnectionIsClosed();

    thenLoggerIsRemovedByServer();
}

TEST_F(LogServer, streaming_logger_removal_before_connection_closure_is_properly_handled)
{
    givenStreamingLogger();

    whenLoggerIsRemoved();
    whenServerConnectionIsClosed();

    // thenSeverDoesNotCrash();
}

} // namespace nx::network::maintenance::log::test
