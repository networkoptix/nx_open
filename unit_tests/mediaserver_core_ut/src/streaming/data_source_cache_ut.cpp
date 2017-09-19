#include <memory>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/uuid.h>

#include <streaming/data_source_cache.h>
#include <streaming/streaming_params.h>

namespace test {

namespace {

class DummyDataProvider:
    public AbstractOnDemandDataProvider
{
public:
    virtual bool tryRead(QnAbstractDataPacketPtr* const /*data*/) override
    {
        return false;
    }

    virtual quint64 currentPos() const override
    {
        return 0;
    }

    virtual void put(QnAbstractDataPacketPtr /*packet*/) override
    {
    }
};

class DummyTranscoder:
    public QnTranscoder
{
public:
    virtual int setContainer(const QString& /*value*/) override
    {
        return 0;
    }

protected:
    virtual int open(
        const QnConstCompressedVideoDataPtr& /*video*/,
        const QnConstCompressedAudioDataPtr& /*audio*/) override
    {
        return 0;
    }

    virtual int transcodePacketInternal(
        const QnConstAbstractMediaDataPtr& /*media*/,
        QnByteArray* const /*result*/) override
    {
        return 0;
    }

    virtual int finalizeInternal(QnByteArray* const /*result*/) override
    {
        return 0;
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

class StreamingDataSourceCache:
    public ::testing::Test
{
public:
    StreamingDataSourceCache():
        m_cache(&m_timerManager)
    {
        m_timerManager.start();
    }

    ~StreamingDataSourceCache()
    {
        m_timerManager.stop();
    }

protected:
    void givenCachedDataSourceNotBoundToASession()
    {
        addDataSourceWithSessionId("");
    }

    void givenCachedDataSourceBoundToSomeSession()
    {
        addDataSourceWithSessionId(nx::utils::generateRandomName(7));
    }

    void whenRequestDataSourceWithSameParameters()
    {
        m_prevCacheResult = m_cache.take(m_prevGeneratedCacheKey);
    }

    void thenDataSourceIsProvided()
    {
        ASSERT_NE(nullptr, m_prevCacheResult);
    }

    void thenNoDataSourceIsProvided()
    {
        ASSERT_EQ(nullptr, m_prevCacheResult);
    }

    void thenExistingTranscoderIsProvided()
    {
        thenDataSourceIsProvided();
        ASSERT_NE(nullptr, m_prevCacheResult->transcoder);
    }

private:
    nx::utils::StandaloneTimerManager m_timerManager;
    ::DataSourceCache m_cache;
    StreamingChunkCacheKey m_prevGeneratedCacheKey;
    DataSourceContextPtr m_prevCacheResult;

    void addDataSourceWithSessionId(QByteArray sessionId)
    {
        std::multimap<QString, QString> auxiliaryParams;
        if (!sessionId.isEmpty())
        {
            auxiliaryParams.emplace(
                StreamingParams::SESSION_ID_PARAM_NAME,
                QString::fromUtf8(sessionId));
        }

        m_prevGeneratedCacheKey = StreamingChunkCacheKey(
            QnUuid::createUuid().toString(), //< uniqueResourceID,
            0, //< channel,
            "video/mp2t", //< containerFormat,
            QString(), //< alias,
            0, //< startTimestamp,
            std::chrono::seconds(10), //< duration,
            MediaQuality::MEDIA_Quality_High, //< streamQuality,
            std::move(auxiliaryParams));

        auto dataSourceContext = std::make_shared<DataSourceContext>(
            std::make_shared<DummyDataProvider>(),
            std::make_unique<DummyTranscoder>());

        m_cache.put(m_prevGeneratedCacheKey, dataSourceContext);
    }
};

TEST_F(StreamingDataSourceCache, transcoders_not_bound_to_a_session_are_always_different)
{
    givenCachedDataSourceNotBoundToASession();
    whenRequestDataSourceWithSameParameters();
    thenNoDataSourceIsProvided();
}

TEST_F(StreamingDataSourceCache, cache_requests_bound_to_the_same_session_provide_transcoder)
{
    givenCachedDataSourceBoundToSomeSession();
    whenRequestDataSourceWithSameParameters();
    thenExistingTranscoderIsProvided();
}

} // namespace test
