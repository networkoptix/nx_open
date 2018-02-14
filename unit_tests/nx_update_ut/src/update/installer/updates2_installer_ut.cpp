#include <nx/update/installer/detail/updates2_installer_base.h>
#include <nx/utils/std/future.h>
#include <nx/utils/raii_guard.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace nx {
namespace update {
namespace detail {
namespace test {

using namespace ::testing;

namespace {

static const QString kDataDirPath = "/dataDir";
static const QString kUpdateZipFilePath = "/downloads/update.zip";

} // namespace

class TestUpdates2Installer: public Updates2InstallerBase
{
public:
    MOCK_CONST_METHOD0(dataDirectoryPath, QString());
    MOCK_METHOD0(cleanInstallerDirectory, bool());
    MOCK_CONST_METHOD0(createZipExtractor, AbstractZipExtractorPtr());
};

enum class ExpectedPrepareOutcome
{
    success,
    fail_noFreeSpace,
    fail_unknown,
    fail_cleanFailed,
    fail_alreadyRunning
};

class TestZipExtractor: public AbstractZipExtractor
{
public:
    TestZipExtractor(ExpectedPrepareOutcome expectedOutcome):
        m_expectedOutcome(expectedOutcome)
    {}

    virtual void extractAsync(
        const QString& /*filePath*/,
        const QString& /*outputDir*/,
        ExtractHandler extractHandler) override
    {
        {
            QnMutexLocker lock(&m_mutex);
            ++m_pendingRequests;
        }

        std::thread(
            [this, self = this, extractHandler]()
            {
                auto decReqCountGuard = QnRaiiGuard::createDestructible(
                    [self]()
                    {
                        QnMutexLocker lock(&self->m_mutex);
                        --self->m_pendingRequests;
                        self->m_condition.wakeOne();
                    });

                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                switch (m_expectedOutcome)
                {
                case ExpectedPrepareOutcome::success:
                    return extractHandler(QnZipExtractor::Error::Ok);
                case ExpectedPrepareOutcome::fail_noFreeSpace:
                    return extractHandler(QnZipExtractor::Error::NoFreeSpace);
                case ExpectedPrepareOutcome::fail_cleanFailed:
                    NX_ASSERT(false);
                    break;
                case ExpectedPrepareOutcome::fail_alreadyRunning:
                {
                    QnMutexLocker lock(&self->m_mutex);
                    while (!m_mayProceed)
                        m_proceedCondition.wait(lock.mutex());
                    return extractHandler(QnZipExtractor::Error::Ok);
                }
                case ExpectedPrepareOutcome::fail_unknown:
                    NX_ASSERT(false);
                    break;
                }

            }).detach();
    }

    virtual ~TestZipExtractor() override
    {
        stop();
    }

    void allowProceed()
    {
        QnMutexLocker lock(&m_mutex);
        m_mayProceed = true;
        m_proceedCondition.wakeOne();
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    QnWaitCondition m_proceedCondition;
    int m_pendingRequests = 0;
    bool m_mayProceed = false;
    ExpectedPrepareOutcome m_expectedOutcome = ExpectedPrepareOutcome::fail_unknown;

    void stop()
    {
        QnMutexLocker lock(&m_mutex);
        while (m_pendingRequests != 0)
            m_condition.wait(lock.mutex());
    }
};

class Updates2Installer: public ::testing::Test
{
protected:
    virtual void TearDown() override
    {
        m_updates2Installer.stopSync();
    }

    void whenPrepareRequestIsIssued(
        ExpectedPrepareOutcome expectedOutcome,
        nx::utils::future<PrepareResult>* future = nullptr)
    {
        initializeExpectations(expectedOutcome);

        auto readyPromise = std::make_shared<nx::utils::promise<PrepareResult>>();
        if (future == nullptr)
            m_readyFuture = readyPromise->get_future();
        else
            *future = readyPromise->get_future();

        m_updates2Installer.prepareAsync(
            "",
            [this, readyPromise](PrepareResult resultCode)
            {
                readyPromise->set_value(resultCode);
            });
    }

    PrepareResult whenPrepareRequestIsIssuedInstantResult(ExpectedPrepareOutcome expectedOutcome)
    {
        nx::utils::future<PrepareResult> readyFuture;
        whenPrepareRequestIsIssued(expectedOutcome, &readyFuture);
        return readyFuture.get();
    }

    void thenPrepareShouldEndWith(PrepareResult prepareResult)
    {
        m_prepareResult = m_readyFuture.get();
        thenResultShouldBe(prepareResult, m_prepareResult);
    }

    void thenResultShouldBe(PrepareResult expectedResult, PrepareResult actualResult)
    {
        ASSERT_EQ(expectedResult, actualResult);
    }

    void thenPrepareShouldAtLastEndWith(PrepareResult prepareResult)
    {
        m_extractorRef->allowProceed();
        while (m_readyFuture.wait_for(std::chrono::milliseconds(5)) != std::future_status::ready);
        ASSERT_EQ(prepareResult, m_readyFuture.get());
    }

private:
    TestUpdates2Installer m_updates2Installer;
    PrepareResult m_prepareResult = PrepareResult::unknownError;
    nx::utils::future<PrepareResult> m_readyFuture;
    std::shared_ptr<TestZipExtractor> m_extractorRef;

    void initializeExpectations(ExpectedPrepareOutcome expectedOutcome)
    {
        auto prepareCalls =
            [this, expectedOutcome](int cleanInstallCount, AbstractZipExtractorPtr extractor)
            {
                EXPECT_CALL(m_updates2Installer, dataDirectoryPath())
                    .Times(1)
                    .WillOnce(Return(kDataDirPath));
                EXPECT_CALL(m_updates2Installer, cleanInstallerDirectory())
                    .Times(cleanInstallCount)
                    .WillRepeatedly(Return(true));
                EXPECT_CALL(m_updates2Installer, createZipExtractor())
                    .Times(1)
                    .WillOnce(Return(extractor));
            };

        if (!m_extractorRef)
            m_extractorRef = std::make_shared<TestZipExtractor>(expectedOutcome);

        switch (expectedOutcome)
        {
        case ExpectedPrepareOutcome::success:
            prepareCalls(1, m_extractorRef);
            break;
        case ExpectedPrepareOutcome::fail_noFreeSpace:
            prepareCalls(2, m_extractorRef);
            break;
        case ExpectedPrepareOutcome::fail_cleanFailed:
            EXPECT_CALL(m_updates2Installer, createZipExtractor())
                .Times(1)
                .WillOnce(Return(m_extractorRef));
            EXPECT_CALL(m_updates2Installer, cleanInstallerDirectory())
                .Times(1)
                .WillOnce(Return(false));
            break;
        case ExpectedPrepareOutcome::fail_alreadyRunning:
            NX_ASSERT(m_extractorRef);
            break;
        case ExpectedPrepareOutcome::fail_unknown:
            NX_ASSERT(false);
            break;
        }
    }
};

TEST_F(Updates2Installer, prepareSuccessful)
{
    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::success);
    thenPrepareShouldEndWith(PrepareResult::ok);
}

TEST_F(Updates2Installer, prepareFailed_noFreeSpace)
{
    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::fail_noFreeSpace);
    thenPrepareShouldEndWith(PrepareResult::noFreeSpace);
}

TEST_F(Updates2Installer, prepareFailed_cleanFailed)
{
    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::fail_cleanFailed);
    thenPrepareShouldEndWith(PrepareResult::cleanTemporaryFilesError);
}

TEST_F(Updates2Installer, prepare_issuedWhenAlreadyRunning_shouldSuccess)
{
    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::success);
    thenResultShouldBe(
        PrepareResult::alreadyStarted,
        whenPrepareRequestIsIssuedInstantResult(ExpectedPrepareOutcome::fail_alreadyRunning));
    thenPrepareShouldAtLastEndWith(PrepareResult::ok);
}

} // namespace test
} // namespace detail
} // namespace update
} // namespace nx
