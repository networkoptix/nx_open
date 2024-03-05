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
        if (m_uploader)
            m_uploader->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        startCollector();

        m_sessionId = nx::Uuid::createUuid().toSimpleStdString();

        nx::log::LevelSettings filter;
        filter.primary = nx::log::Level::verbose;
        filter.filters.emplace(
            std::string("nx::network::maintenance::log::test"),
            nx::log::Level::verbose);

        m_uploader = std::make_unique<Uploader>(collectorUrl(), m_sessionId, filter);
        m_uploader->setMinBufSizeToUpload(/*do not accumulate data before uploading*/ 0);
    }

    void givenStartedUploader()
    {
        givenStartedUploaderWithTimeout(std::nullopt);
    }

    void givenStartedUploaderWithTimeout(std::optional<std::chrono::milliseconds> timeout)
    {
        m_uploader->start(timeout, [this](auto&&... args) {
            saveUploadResult(std::forward<decltype(args)>(args)...);
        });
    }

    void whenGenerateSomeLogs()
    {
        for (int i = 0; i < 7; ++i)
            generateLogMessage();
    }

    void whenGenerateLogsOfSize(std::size_t size)
    {
        for (std::size_t totalSize = 0; totalSize < size; )
        {
            generateLogMessage();
            totalSize += m_generatedLogs.back().size();
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

    void thenSomeLogsAreUploadedEventually()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        for (;;)
        {
            auto& uploadedLogs = m_uploadedLogs[m_sessionId];
            if (!uploadedLogs.empty())
                break;

            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            lock.relock();
        }

        auto& uploadedLogs = m_uploadedLogs[m_sessionId];

        const auto found = std::all_of(
            uploadedLogs.begin(), uploadedLogs.end(),
            [this](const std::string& uploaded)
            {
                return std::any_of(
                    m_generatedLogs.begin(), m_generatedLogs.end(),
                    [&uploaded](const std::string& generated) {
                        return uploaded.find(generated) != std::string::npos;
                    });
            });

        ASSERT_TRUE(found);
    }

    void thenUploaderCompleted()
    {
        m_lastUploadResult = m_uploadResults.pop();
    }

    template<typename F>
    void andEachUploadedFragment(F f)
    {
        for (const auto& fragment: m_uploadedFragments)
            f(fragment);
    }

    UploadResult uploaderResult()
    {
        return *m_lastUploadResult;
    }

    void setCollectorMalfunctioning(http::StatusCode::Value val)
    {
        m_collectorForcedResponse = val;
    }

    Uploader& uploader()
    {
        return *m_uploader;
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
            m_uploadedFragments.push_back(str);
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

    void generateLogMessage()
    {
        m_generatedLogs.push_back(nx::format("Log uploader test message %1",
            nx::utils::generateRandomName(15)).toStdString());

        NX_VERBOSE(this, m_generatedLogs.back());
    }

private:
    std::vector<std::string> m_generatedLogs;
    std::unique_ptr<Uploader> m_uploader;
    std::string m_sessionId;
    nx::Mutex m_mutex;
    std::unordered_map<std::string /*sessionId*/, std::vector<std::string>> m_uploadedLogs;
    std::vector<std::string> m_uploadedFragments;
    std::optional<http::StatusCode::Value> m_collectorForcedResponse;
    nx::utils::SyncQueue<UploadResult> m_uploadResults;
    std::optional<UploadResult> m_lastUploadResult;
    http::TestHttpServer m_collector;
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

TEST_F(LogUploader, data_is_accumulated_before_upload)
{
    static const std::size_t uploadChunkSize = 1024;

    uploader().setAccumulateDataTimeout(std::chrono::hours(100));
    uploader().setMinBufSizeToUpload(uploadChunkSize);

    givenStartedUploader();

    whenGenerateLogsOfSize(10 * uploadChunkSize);
    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    thenSomeLogsAreUploadedEventually();
    andEachUploadedFragment(
        [](const auto& fragment) { ASSERT_GE(fragment.size(), uploadChunkSize); });
}

} // namespace nx::network::maintenance::log::test
