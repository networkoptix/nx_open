#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/maintenance/log/client.h>
#include <nx/network/maintenance/log/request_path.h>
#include <nx/network/maintenance/log/utils.h>
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
public:
    ~LogServer()
    {
        delete m_httpClient;
        delete m_httpServer;
        delete m_logServer;
    }
protected:
    virtual void SetUp() override
    {
        m_httpServer = new http::TestHttpServer();
        ASSERT_TRUE(m_httpServer->bindAndListen());

        m_httpClient = new http::HttpClient();
        m_httpClient->setResponseReadTimeout(kNoTimeout);
        m_httpClient->setMessageBodyReadTimeout(kNoTimeout);

        m_logServer = new Server(&m_loggerCollection);

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
        Loggers loggersReturnedByServer = QJson::deserialized<Loggers>(*messageBody, {}, &parseSucceeded);
        ASSERT_TRUE(parseSucceeded);

        ASSERT_EQ(loggers.size(), loggersReturnedByServer.loggers.size());
        for (int i = 0; i < loggersReturnedByServer.loggers.size(); ++i)
        {
            assertLoggerEquality(
                loggers[i], 
                loggersReturnedByServer.loggers[i],
                /*compareId*/ true,
                /*comparePath*/ true);
        }
    }

    void andAddloggerToAddFailed()
    {
        ASSERT_EQ(http::StatusCode::badRequest, m_httpClient->response()->statusLine.statusCode);
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

    void assertLoggerEquality(const Logger& a, const Logger& b, bool compareId, bool comparePath)
    {
        if (compareId)
        {
            ASSERT_EQ(a.id, b.id);
        }

        if (comparePath)
        {
            ASSERT_EQ(a.path, b.path);
        }

        assertDefaultLevelEquality(a, b);
        assertFiltersEquality(a, b);
    }

    void assertDefaultLevelEquality(const Logger& loggerToAdd, const Logger& loggerReturnedByServer)
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

    void whenAddLoggerStreamingConfiguration(Level level, const std::string& tag)
    {
        std::string levelStr = toString(level).toStdString();
        std::string query = std::string("level=" + levelStr + "[") + tag.c_str() + "],level=none";
        ASSERT_TRUE(m_httpClient->doGet(createRequestUrl(kStream, query)));
    }

    void thenLogsAreProduced(
        const Level level,
        const std::string& tagName,
        int logsToProduce,
        const std::string& logThisString = std::string())
    {
        Tag tag(tagName);

        for (int i = 0; i < logsToProduce; ++i)
        {
            if (auto logger = loggerCollection()->get(tag, true))
                logger->log(level, tag, QString::number(i) + '\n');
        }

        if (!logThisString.empty())
        {
            if (auto logger = loggerCollection()->get(tag, true))
                logger->log(level, tag, QString(logThisString.c_str()) + '\n');
        }
    }

    void andLogStreamIsFetched(const std::string& stringToFind)
    {
        QString s;
        while (!s.contains(stringToFind.c_str()))
            s += m_httpClient->fetchMessageBodyBuffer();
    }

    void andLoggerIsRemoved(int loggerId)
    {
        loggerCollection()->remove(loggerId);
    }

    void whenAddLoggingConfigurationWithMalformedJson()
    {
        ASSERT_TRUE(m_httpClient->doPost(
            createRequestUrl(kLoggers),
            "application/json",
            getMalformedJson().toUtf8()));
    }

    void whenRequestLogStreamWithMalformeQueryString()
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
    http::TestHttpServer* m_httpServer = nullptr;
    http::HttpClient* m_httpClient = nullptr;
    maintenance::log::Server* m_logServer = nullptr;

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

TEST_F(LogServer, server_provides_all_logger_configurations)
{
    std::vector<Logger> loggersReturnedByServer;
    givenTwoLoggers(&loggersReturnedByServer);

    whenRequestLogConfiguration();

    thenRequestSucceeded(http::StatusCode::ok);
    andActualConfigurationIsProvided(loggersReturnedByServer);
}

TEST_F(LogServer, server_accepts_new_logger)
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
    server_accepts_loggers_with_a_duplicate_tag_and_second_logger_does_not_have_duplicate)
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

TEST_F(LogServer, server_rejects_logger_configuration_with_duplicate_tags)
{    
    Logger loggerToAdd = getDefaultLogger();
    Logger duplicateLoggerToAdd = loggerToAdd;


    whenAddLoggerConfiguration(loggerToAdd);

    thenRequestSucceeded(http::StatusCode::created);


    whenAddLoggerConfiguration(duplicateLoggerToAdd);
    
    thenRequestFailed(http::StatusCode::badRequest);
}

TEST_F(LogServer, server_deletes_existing_logger_configuration)
{
    Logger loggerToAdd = getDefaultLogger();
    Logger loggerReturnedByServer;

    whenAddLoggerConfiguration(getDefaultLogger());
    thenRequestSucceeded(http::StatusCode::created);

    andNewLoggerConfigurationIsProvided(
        loggerToAdd,
        /*compareFilters*/ true,
        &loggerReturnedByServer);
    
    whenDeleteLoggerConfiguration(loggerReturnedByServer.id);
    thenRequestSucceeded(http::StatusCode::ok);
}                                    

TEST_F(LogServer, server_fails_to_delete_non_existing_logger_configuration)
{
    givenTwoLoggers();

    whenDeleteLoggerConfiguration(2 /*loggerId*/);
    thenRequestFailed(http::StatusCode::notFound);
}

TEST_F(LogServer, server_streams_logs_by_adding_custom_logging_configuration)
{
    Level level(Level::debug);
    std::string tag("nx::network");
    std::string targetString("find me");
    int logsToProduce = 20;

    whenAddLoggerStreamingConfiguration(level, tag);

    thenRequestSucceeded(http::StatusCode::ok);
    thenLogsAreProduced(level, tag, logsToProduce, targetString);
    andLogStreamIsFetched(targetString);
}

TEST_F(LogServer, server_rejects_malformated_json_when_adding_logger_configuration)
{
    whenAddLoggingConfigurationWithMalformedJson();
    
    thenRequestFailed(http::StatusCode::badRequest);
}

TEST_F(LogServer, server_rejects_logger_stream_request_with_malformed_query_string)
{
    whenRequestLogStreamWithMalformeQueryString();

    thenRequestFailed(http::StatusCode::badRequest);
}

TEST_F(LogServer, DISABLED_server_crashes_when_logger_is_removed_during_stream)
{
    Level level(Level::debug);
    std::string tag("nx::network");

    whenAddLoggerStreamingConfiguration(level, tag);
    thenRequestSucceeded(http::StatusCode::ok);

    std::thread thread([&]()
    {
        thenLogsAreProduced(level, tag, 10000);
    });

    andLoggerIsRemoved(0);

    thread.join();
}

} // namespace nx::network::maintenance::log::test
