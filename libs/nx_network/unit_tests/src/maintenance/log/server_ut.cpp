#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/maintenance/log/client.h>
#include <nx/network/maintenance/log/utils.h>
#include <nx/network/maintenance/log/server.h>
#include <nx/network/maintenance/log/request_path.h>

#include <nx/utils/log/logger_collection.h>
#include <nx/utils/log/log_level.h>


namespace nx::network::maintenance::log::test {

using namespace nx::utils::log;

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

        m_httpClient = std::make_unique<http::HttpClient>();
        m_httpClient->setResponseReadTimeout(kNoTimeout);
        m_httpClient->setMessageBodyReadTimeout(kNoTimeout);

        m_logServer = std::make_unique<maintenance::log::Server>(loggerCollection());

        m_logServer->registerRequestHandlers(
            kBasePath,
            &m_httpServer->httpMessageDispatcher());
    }

    void whenRequestLogConfiguration()
    {
        ASSERT_TRUE(m_httpClient->doGet(createRequestUrl(kLoggers)));
    }

    void andActualConfigurationIsProvided(const std::vector<Logger>& loggers)
    {
        const auto messageBody = m_httpClient->fetchEntireMessageBody();
        ASSERT_TRUE(messageBody);

        bool parseSucceeded = false;
        Loggers loggersReturnedByServer = QJson::deserialized<Loggers>(
            *messageBody,
            Loggers(),
            &parseSucceeded);
        ASSERT_TRUE(parseSucceeded);

        ASSERT_EQ(loggers.size(), loggersReturnedByServer.loggers.size());
        for (std::size_t i = 0; i < loggersReturnedByServer.loggers.size(); ++i)
            assertLoggerEquality(loggers[i], loggersReturnedByServer.loggers[i]);
    }

    void whenAddLoggerConfiguration(const Logger& logger)
    {
        ASSERT_TRUE(m_httpClient->doPost(
            createRequestUrl(kLoggers),
            "application/json",
            QJson::serialized(logger)));
    }

    void andNewLoggerConfigurationIsProvided(
        const Logger& loggerToAdd,
        bool compareFilters,
        Logger* outLoggerReturnedByServer)
    {
        const auto messageBody = m_httpClient->fetchEntireMessageBody();
        ASSERT_TRUE(messageBody);

        bool parseSucceeded = false;
        Logger loggerReturnedByServer = QJson::deserialized<Logger>(*messageBody, {}, &parseSucceeded);
        ASSERT_TRUE(parseSucceeded);

        assertDefaultLevelEquality(loggerToAdd, loggerReturnedByServer);
        if (compareFilters)
            assertFiltersEquality(loggerToAdd, loggerReturnedByServer);

        if (outLoggerReturnedByServer)
            *outLoggerReturnedByServer = loggerReturnedByServer;
    }

    void assertLoggerEquality(const Logger& a, const Logger& b)
    {
        ASSERT_EQ(a.id, b.id);
        ASSERT_EQ(a.path, b.path);
        assertDefaultLevelEquality(a, b);
        assertFiltersEquality(a, b);
    }

    void assertDefaultLevelEquality(
        const Logger& loggerToAdd,
        const Logger& loggerReturnedByServer)
    {
        // When adding a logger, empty defaultLevel is interpreted as Level::none by server.
        if (loggerReturnedByServer.defaultLevel == "none")
        {
            ASSERT_TRUE(loggerToAdd.defaultLevel == "none" || loggerToAdd.defaultLevel.empty());
        }
        else
        {
            ASSERT_EQ(loggerReturnedByServer.defaultLevel, loggerToAdd.defaultLevel);
        }
    }

    void assertFiltersEquality(const Logger& a, const Logger& b)
    {
        LevelFilters aFilters = utils::toLevelFilters(a.filters);
        LevelFilters bFilters = utils::toLevelFilters(b.filters);
        ASSERT_EQ(aFilters.size(), bFilters.size());
        for (const auto& element : aFilters)
        {
            auto it = bFilters.find(element.first);
            ASSERT_TRUE(it != bFilters.end());
            ASSERT_EQ(it->first.toString(), element.first.toString());
            ASSERT_EQ(it->second, element.second);
        }
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
        ASSERT_TRUE(m_httpClient->doDelete(createDeleteRequestUrl(kLoggers, loggerId)));
    }

    void whenDeleteLoggerUsingUnknownId()
    {
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

    void givenTwoLoggers(std::vector<Logger>* outloggers = nullptr)
    {
        Logger loggerReturnedByServer;
        Logger loggerToAdd1 = getDefaultLogger();
        Logger loggerToAdd2 = getDefaultLogger2();

        // Logger 1.
        whenAddLoggerConfiguration(loggerToAdd1);

        thenRequestSucceeded(http::StatusCode::created);
        andNewLoggerConfigurationIsProvided(loggerToAdd1, false, &loggerReturnedByServer);

        if (outloggers)
            outloggers->push_back(loggerReturnedByServer);

        // Logger 2.
        whenAddLoggerConfiguration(loggerToAdd2);

        thenRequestSucceeded(http::StatusCode::created);
        andNewLoggerConfigurationIsProvided(loggerToAdd2, false, &loggerReturnedByServer);

        if (outloggers)
            outloggers->push_back(loggerReturnedByServer);
    }

    void whenAddLoggerStreamingConfiguration()
    {
        std::string query("level=debug[nx::network],level=none");
        ASSERT_TRUE(m_httpClient->doGet(createRequestUrl(kStream, query)));
    }

    void givenStreamingLogger()
    {
        whenAddLoggerStreamingConfiguration();
        thenRequestSucceeded(http::StatusCode::ok);
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

    void thenLogsAreProduced()
    {
        std::string tagStr(kTag);
        Tag tag(tagStr);

        for (int i = 0; i < 20; ++i)
        {
            if (auto logger = loggerCollection()->get(tag, true))
                logger->log(kLevel, tag, QString::number(i) + '\n');
        }

        if (auto logger = loggerCollection()->get(tag, true))
            logger->log(kLevel, tag, QString(kTargetString) + '\n');
    }

    void andLogStreamIsFetched()
    {
        QString s;
        while (!s.contains(kTargetString))
            s += m_httpClient->fetchMessageBodyBuffer();
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
            getMalformedJson().toUtf8()));
    }

    void whenRequestLogStreamWithMalformedQueryString()
    {
        std::string malformedJson = "level=verbnx::network],level=none"; //< "verb" instead of "verbose"
        ASSERT_TRUE(m_httpClient->doGet(createRequestUrl(kStream, malformedJson)));
    }

    LoggerCollection* loggerCollection()
    {
        return &m_loggerCollection;
    }

    Logger getDefaultLogger(
        const std::string & tag = "nx::network",
        const std::string& filePath = "C:/develop/some_log_file")
    {
        Filter f;
        f.level = "verbose";
        f.tags = { tag };

        Filter f1;
        f1.level = "info";
        f1.tags = { "nx::utils", "nx::utils::log" };

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

    static QString getMalformedJson()
    {
        return QString("{")
            + "\"file : \"/path/to/log/file\"," //< Missing \" after file
            + "\"filters\": ["
            + "{\"level\": \"verbose\", \"tags\": [\"nx::network\", \"nx::utils\"]},"
            + "{ \"level\": \"none\"}"
            + "]"
            + ""; //< Should be "}"
    }

private:
    LoggerCollection m_loggerCollection;
    std::unique_ptr<http::TestHttpServer> m_httpServer;
    std::unique_ptr<http::HttpClient> m_httpClient;
    std::unique_ptr<maintenance::log::Server> m_logServer;

    nx::utils::Url createRequestUrl(
        const std::string& requestName,
        const std::string& query = std::string())
    {
        url::Builder builder;
        builder.setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer->serverAddress())
            .setPath(kBasePath).appendPath(requestName);
        if (!query.empty())
            builder.setQuery(query.c_str());
        return builder;
    }

    nx::utils::Url createDeleteRequestUrl(const std::string& requestName, int loggerId)
    {
        return url::Builder().setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer->serverAddress())
            .setPath(kBasePath).appendPath(requestName)
            .appendPath("/").appendPath(std::to_string(loggerId));
    }
    };

TEST_F(LogServer, provides_all_logger_configurations)
{
    std::vector<Logger> loggersReturnedByServer;
    givenTwoLoggers(&loggersReturnedByServer);

    whenRequestLogConfiguration();

    thenRequestSucceeded(http::StatusCode::ok);
    andActualConfigurationIsProvided(loggersReturnedByServer);
}

TEST_F(LogServer, accepts_new_logger)
{
    Logger loggerToAdd = getDefaultLogger("nx::network");
    Logger loggerReturnedByServer;

    whenAddLoggerConfiguration(loggerToAdd);

    thenRequestSucceeded(http::StatusCode::created);
    andNewLoggerConfigurationIsProvided(
        loggerToAdd,
        /*compareFilters*/ true,
        &loggerReturnedByServer);
}

TEST_F(
    LogServer,
    accepts_loggers_with_a_duplicate_tag_and_second_logger_does_not_have_duplicate)
{
    std::string duplicateTag("nx::network");
    Logger loggerToAdd1 = getDefaultLogger(duplicateTag);
    Logger loggerToAdd2 = getDefaultLogger2(duplicateTag);
    Logger loggerReturnedByServer1;
    Logger loggerReturnedByServer2;


    whenAddLoggerConfiguration(loggerToAdd1);

    thenRequestSucceeded(http::StatusCode::created);
    andNewLoggerConfigurationIsProvided(
        loggerToAdd1,
        /*compareFilters*/ true,
        &loggerReturnedByServer1);


    whenAddLoggerConfiguration(loggerToAdd2);

    thenRequestSucceeded(http::StatusCode::created);
    andNewLoggerConfigurationIsProvided(
        loggerToAdd2,
        /*compareFilters*/ false,
        &loggerReturnedByServer2);


    assertLoggerHasTag(loggerReturnedByServer1, duplicateTag);
    assertLoggerDoesNotHaveTag(loggerReturnedByServer2, duplicateTag);
}

TEST_F(LogServer, rejects_logger_configuration_with_duplicate_tags)
{
    Logger loggerToAdd = getDefaultLogger();
    Logger duplicateLoggerToAdd = loggerToAdd;


    whenAddLoggerConfiguration(loggerToAdd);

    thenRequestSucceeded(http::StatusCode::created);


    whenAddLoggerConfiguration(duplicateLoggerToAdd);

    thenRequestFailed(http::StatusCode::badRequest);
}

TEST_F(LogServer, deletes_existing_logger_configuration)
{
    Logger loggerToAdd = getDefaultLogger();
    Logger loggerReturnedByServer;

    whenAddLoggerConfiguration(loggerToAdd);
    thenRequestSucceeded(http::StatusCode::created);

    andNewLoggerConfigurationIsProvided(
        loggerToAdd,
        /*compareFilters*/ true,
        &loggerReturnedByServer);

    whenDeleteLoggerConfiguration(loggerReturnedByServer.id);
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
