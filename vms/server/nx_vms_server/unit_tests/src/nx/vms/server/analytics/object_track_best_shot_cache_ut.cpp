#include <gtest/gtest.h>

#include <nx/utils/time/time_provider.h>
#include <nx/vms/server/analytics/object_track_best_shot_cache.h>

namespace nx::vms::server::analytics {

using namespace std::chrono;
using namespace nx::utils::time;

using Image = nx::analytics::db::Image;

static const seconds kMaxCacheDuration(1);
static const QnUuid kTrackId = QnUuid::fromStringSafe("{3a628abc-59e5-4525-8ebb-41ffb2a6e658}");
static const int kLengthOfBestShotSequence = 5;

class TestTimeProvider: public AbstractTimeProvider
{
public:
    TestTimeProvider(microseconds initialTime, microseconds tick):
        m_currentTime(std::move(initialTime)),
        m_tick(std::move(tick))
    {
    }

    virtual microseconds currentTime() const override { return m_currentTime; }

    void setTime(microseconds time) { m_currentTime = std::move(time); }

    void setTick(microseconds tick) { m_tick = std::move(tick); }

    void tick() { m_currentTime += m_tick; }

private:
    microseconds m_currentTime{0};
    microseconds m_tick{1};
};

class ObjectTrackBestShotCacheTest: public ::testing::Test
{
private:
    struct BestShot
    {
        QnUuid trackId;
        Image image;
        microseconds timestamp;
    };

protected:
    virtual void SetUp()
    {
        m_timeProvider = std::make_unique<TestTimeProvider>(seconds(0), milliseconds(33));
        m_objectTrackBestShotCache = std::make_unique<ObjectTrackBestShotCache>(
            kMaxCacheDuration,
            m_timeProvider.get());
    }

    void insertBestShot()
    {
        BestShot bestShot;
        Image image;
        image.imageData = NX_FMT("image_data_%1", m_timeProvider->currentTime()).toUtf8();
        image.imageDataFormat = "image/jpeg";

        bestShot.image = std::move(image);
        bestShot.trackId = QnUuid::createUuid();
        bestShot.timestamp = m_timeProvider->currentTime();

        m_bestShots.push_back(bestShot);
        m_objectTrackBestShotCache->insert(bestShot.trackId, bestShot.image);
    }

    void insertOneMoreBestShot()
    {
        BestShot bestShot;
        Image image;
        image.imageData = "last_image_data";
        image.imageDataFormat = "image/jpeg";

        bestShot.image = std::move(image);
        bestShot.trackId = QnUuid::createUuid();
        bestShot.timestamp = m_timeProvider->currentTime();

        m_lastBestShot = bestShot;
        m_objectTrackBestShotCache->insert(bestShot.trackId, bestShot.image);
    }

    void insertBestShotsWithTheSameTimestamp()
    {
        for (int i = 0; i < kLengthOfBestShotSequence; ++i)
            insertBestShot();
    }

    void insertBestShotsWithIncreasingTimestamps()
    {
        for (int i = 0; i < kLengthOfBestShotSequence; ++i)
        {
            insertBestShot();
            m_timeProvider->tick();
        }
    }

    void waitHalfOfCacheExpirationPeriod()
    {
        m_timeProvider->setTime(kMaxCacheDuration / 2);
    }

    void waitTillCacheIsExpired()
    {
        m_timeProvider->setTime(m_bestShots.back().timestamp + kMaxCacheDuration + 1us);
    }

    void makeSureAllBestShotsCanBeFetched()
    {
        for (const BestShot& bestShot: m_bestShots)
        {
            const std::optional<Image> image = m_objectTrackBestShotCache->fetch(bestShot.trackId);
            ASSERT_TRUE(image);
            ASSERT_EQ(image->imageData, bestShot.image.imageData);
        }
    }

    void makeSureOldBestShotsAreExpired()
    {
        for (const BestShot& bestShot: m_bestShots)
        {
            const std::optional<Image> image = m_objectTrackBestShotCache->fetch(bestShot.trackId);
            ASSERT_FALSE(image);
        }
    }

    void makeSureLastBestShotCanBeFetched()
    {
        const std::optional<Image> image =
            m_objectTrackBestShotCache->fetch(m_lastBestShot.trackId);

        ASSERT_TRUE(image);
    }

protected:
    std::unique_ptr<TestTimeProvider> m_timeProvider;
    std::unique_ptr<ObjectTrackBestShotCache> m_objectTrackBestShotCache;

    std::vector<BestShot> m_bestShots;
    BestShot m_lastBestShot;
};

TEST_F(ObjectTrackBestShotCacheTest, fetchOneBestShot)
{
    insertBestShot();
    makeSureAllBestShotsCanBeFetched();
}

TEST_F(ObjectTrackBestShotCacheTest, fetchManyBestShotsWithTheSameTimestamp)
{
    insertBestShotsWithTheSameTimestamp();
    makeSureAllBestShotsCanBeFetched();
}

TEST_F(ObjectTrackBestShotCacheTest, fetchManyBestShotsWithDifferentTimestamps)
{
    insertBestShotsWithIncreasingTimestamps();
    makeSureAllBestShotsCanBeFetched();
}

TEST_F(ObjectTrackBestShotCacheTest, fetchManyBestShotsWithTheSameTimestampAfterDelay)
{
    insertBestShotsWithTheSameTimestamp();
    waitHalfOfCacheExpirationPeriod();
    insertOneMoreBestShot();
    makeSureAllBestShotsCanBeFetched();
    makeSureLastBestShotCanBeFetched();
}

TEST_F(ObjectTrackBestShotCacheTest, fetchManyBestShotsWithDifferentTimestampsAfterDelay)
{
    insertBestShotsWithIncreasingTimestamps();
    waitHalfOfCacheExpirationPeriod();
    insertOneMoreBestShot();
    makeSureAllBestShotsCanBeFetched();
    makeSureLastBestShotCanBeFetched();
}

TEST_F(ObjectTrackBestShotCacheTest, makeSureBestShotsWithTheSameTimestampAreExpired)
{
    insertBestShotsWithTheSameTimestamp();
    waitTillCacheIsExpired();
    insertOneMoreBestShot();
    makeSureOldBestShotsAreExpired();
    makeSureLastBestShotCanBeFetched();
}

TEST_F(ObjectTrackBestShotCacheTest, makeSureBestShotsWithDifferentTimestampsAreExpired)
{
    insertBestShotsWithIncreasingTimestamps();
    waitTillCacheIsExpired();
    insertOneMoreBestShot();
    makeSureOldBestShotsAreExpired();
    makeSureLastBestShotCanBeFetched();
}

} // namespace nx::vms::server::analytics
