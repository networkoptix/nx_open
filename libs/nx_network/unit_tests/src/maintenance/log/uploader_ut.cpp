// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <vector>

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/maintenance/log/collector_api_paths.h>
#include <nx/network/maintenance/log/uploader.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/uuid.h>

namespace nx::network::maintenance::log::test {

class LogUploader:
    public ::testing::Test
{
public:
    ~LogUploader()
    {
        if(m_uploader)
            m_uploader->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        startCollector();
    }

    void givenStartedUploader()
    {
        givenStartedUploaderWithTimeout(std::nullopt);
    }

    void givenStartedUploaderWithTimeout(std::optional<std::chrono::milliseconds> timeout)
    {
        m_sessionId = QnUuid::createUuid().toSimpleStdString();

        nx::utils::log::LevelSettings filter;
        filter.primary = nx::utils::log::Level::verbose;
        filter.filters.emplace(
            std::string("nx::network::maintenance::log::test"),
            nx::utils::log::Level::verbose);

        m_uploader = std::make_unique<Uploader>(collectorUrl(), m_sessionId, filter);
        m_uploader->start(timeout, [this](auto&&... args) {
            saveUploadResult(std::forward<decltype(args)>(args)...);
        });
    }

    void whenGenerateSomeLogs()
    {
        for (int i = 0; i < 7; ++i)
        {
            m_generatedLogs.push_back(nx::utils::generateRandomName(15));
            NX_VERBOSE(this, "Log uploader test message %1", m_generatedLogs.back());
        }
    }

    void thenLogsAreUploadedEventually()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        auto& uploadedLogs = m_uploadedLogs[m_sessionId];

        for (;;)
        {
            const auto found = std::all_of(
                m_generatedLogs.begin(), m_generatedLogs.end(),
                [&uploadedLogs](const std::string& str)
                {
                    return std::any_of(
                        uploadedLogs.begin(), uploadedLogs.end(),
                        [&str](const std::string& uploaded) { return uploaded.find(str) != std::string::npos; });
                });

            if (found)
                break;

            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            lock.relock();
        }
    }

    void thenUploaderCompleted()
    {
        m_lastUploadResult = m_uploadResults.pop();
    }

    UploadResult uploaderResult()
    {
        return *m_lastUploadResult;
    }

    void setCollectorMalfunctioning(http::StatusCode::Value val)
    {
        m_collectorForcedResponse = val;
    }

private:
    void startCollector()
    {
        m_collector.httpMessageDispatcher().registerRequestProcessorFunc(
            http::Method::post,
            kLogSessionFragmentsPath,
            [this](auto&&... args) { serveUploadLog(std::forward<decltype(args)>(args)...); });

        ASSERT_TRUE(m_collector.bindAndListen(SocketAddress::anyPrivateAddress));
    }

    void serveUploadLog(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler)
    {
        if (m_collectorForcedResponse)
            return completionHandler(*m_collectorForcedResponse);

        const std::string sessionId =
            requestContext.requestPathParams.getByName(kLogSessionIdParam);

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            const auto str = requestContext.request.messageBody.toStdString();
            http::StringLineIterator splitter(str);
            for (auto line = splitter.next(); line; line = splitter.next())
                m_uploadedLogs[sessionId].push_back(std::string(*line));
        }

        completionHandler(http::StatusCode::created);
    }

    nx::utils::Url collectorUrl()
    {
        return url::Builder().setScheme(http::kUrlSchemeName)
            .setEndpoint(m_collector.serverAddress()).toUrl();
    }

    void saveUploadResult(UploadResult result)
    {
        m_uploadResults.push(std::move(result));
    }

private:
    http::TestHttpServer m_collector;
    std::vector<std::string> m_generatedLogs;
    std::unique_ptr<Uploader> m_uploader;
    std::string m_sessionId;
    nx::Mutex m_mutex;
    std::unordered_map<std::string /*sessionId*/, std::vector<std::string>> m_uploadedLogs;
    std::optional<http::StatusCode::Value> m_collectorForcedResponse;
    nx::utils::SyncQueue<UploadResult> m_uploadResults;
    std::optional<UploadResult> m_lastUploadResult;
};

TEST_F(LogUploader, log_is_uploaded_to_collector)
{
    givenStartedUploader();
    whenGenerateSomeLogs();
    thenLogsAreUploadedEventually();
}

TEST_F(LogUploader, unsuccessful_upload_is_finished_on_timeout)
{
    setCollectorMalfunctioning(http::StatusCode::forbidden);

    givenStartedUploaderWithTimeout(std::chrono::milliseconds(100));
    whenGenerateSomeLogs();

    thenUploaderCompleted();
    ASSERT_EQ(0, uploaderResult().bytesUploaded);
    ASSERT_GE(uploaderResult().bytesDropped, 0);
    ASSERT_EQ(http::StatusCode::forbidden, uploaderResult().lastHttpError);
}

} // namespace nx::network::maintenance::log::test
