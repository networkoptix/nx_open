#include <gtest/gtest.h>

#include <thread>

#include <nx/vms/auth/time_based_nonce_provider.h>
#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

static const std::chrono::milliseconds kMaxServerTimeDifference(50);
static const std::chrono::milliseconds kSteadyExpirationPeriod(300);

struct TimeBasedNonceProviderTest: ::testing::Test
{
    QnSyncTime syncTime;
    TimeBasedNonceProvider nonceProvider;

    TimeBasedNonceProviderTest():
        nonceProvider(kMaxServerTimeDifference, kSteadyExpirationPeriod)
    {
        NX_INFO(this, lm("Start time: %1").arg(syncTime.currentDateTime()));
    }

    ~TimeBasedNonceProviderTest()
    {
        NX_INFO(this, lm("End time: %1").arg(syncTime.currentDateTime()));
    }

    QByteArray makeNonce(std::chrono::milliseconds timeShift)
    {
        const auto nonceTime = syncTime.currentTimePoint() + timeShift;
        return QByteArray::number((qint64) nonceTime.count(), 16);
    }

    void testNonceExpiration(const QByteArray& nonce)
    {
        QElapsedTimer timer;
        timer.restart();
        const int maxInterval = kSteadyExpirationPeriod.count() * 2;
        while (nonceProvider.isNonceValid(nonce) && timer.elapsed() < maxInterval)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const auto interval = timer.elapsed();
        EXPECT_GT(interval, maxInterval / 2);
        EXPECT_LT(interval, maxInterval * 2);
    }
};

TEST_F(TimeBasedNonceProviderTest, ExpirationOfGeneratedNonce)
{
    testNonceExpiration(nonceProvider.generateNonce());
}

TEST_F(TimeBasedNonceProviderTest, ExpirationOfCloseServerTime)
{
    testNonceExpiration(makeNonce(std::chrono::milliseconds::zero()));
}

TEST_F(TimeBasedNonceProviderTest, RejectedRemoteFutureOrPast)
{
    EXPECT_FALSE(nonceProvider.isNonceValid(makeNonce(kMaxServerTimeDifference * 2)));
    EXPECT_FALSE(nonceProvider.isNonceValid(makeNonce(-kMaxServerTimeDifference * 2)));
}
