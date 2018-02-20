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
    fail_unknown,
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
                    });

                switch (m_expectedOutcome)
                {
                case ExpectedPrepareOutcome::success:
                    return extractHandler(QnZipExtractor::Error::Ok);
                case ExpectedPrepareOutcome::fail_unknown:
                    NX_ASSERT(false);
                    break;
                }
            }).detach();
    }

    virtual void stop() override
    {
        QnMutexLocker lock(&m_mutex);
        while (m_pendingRequests != 0)
            m_condition.wait(lock.mutex());
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    int m_pendingRequests = 0;
    ExpectedPrepareOutcome m_expectedOutcome = ExpectedPrepareOutcome::fail_unknown;
};

class Updates2Installer: public ::testing::Test
{
protected:
    void whenPrepareRequestIsIssued(ExpectedPrepareOutcome expectedOutcome)
    {
        initializePrepareExpectations(expectedOutcome);

        nx::utils::promise<PrepareResult> readyPromise;
        auto readyFuture = readyPromise.get_future();

        m_updates2Installer.prepareAsync(
            "",
            [this, &readyPromise](PrepareResult resultCode)
            {
                readyPromise.set_value(resultCode);
            });

        m_prepareResult = readyFuture.get();
    }

    void thenPrepareShouldEndWith(PrepareResult prepareResult)
    {
        ASSERT_EQ(prepareResult, m_prepareResult);
    }

private:
    TestUpdates2Installer m_updates2Installer;
    PrepareResult m_prepareResult = PrepareResult::unknownError;

    void initializePrepareExpectations(ExpectedPrepareOutcome expectedOutcome)
    {
        switch (expectedOutcome)
        {
        case ExpectedPrepareOutcome::success:
            EXPECT_CALL(m_updates2Installer, dataDirectoryPath())
                .Times(1)
                .WillOnce(Return(kDataDirPath));
            EXPECT_CALL(m_updates2Installer, cleanInstallerDirectory())
                .Times(2)
                .WillRepeatedly(Return(true));
            EXPECT_CALL(m_updates2Installer, createZipExtractor())
                .Times(1)
                .WillOnce(Return(
                    std::make_shared<TestZipExtractor>(ExpectedPrepareOutcome::success)));
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

} // namespace test
} // namespace detail
} // namespace update
} // namespace nx
