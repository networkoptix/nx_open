#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/update/installer/detail/updates2_installer_base.h>
#include <nx/utils/std/future.h>
#include <nx/utils/raii_guard.h>
#include <nx/vms/api/data/system_information.h>

namespace nx {
namespace update {
namespace installer {
namespace detail {
namespace test {

using namespace ::testing;
using nx::vms::api::SystemInformation;

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
    MOCK_CONST_METHOD1(updateInformation, QVariantMap(const QString&));
    MOCK_CONST_METHOD0(systemInformation, SystemInformation());
    MOCK_CONST_METHOD1(checkExecutable, bool(const QString&));
    MOCK_CONST_METHOD2(
        initializeUpdateLog,
        bool(const QString& targetVersion, QString* logFileName));
};

enum class ExpectedPrepareOutcome
{
    success,
    fail_noFreeSpace,
    fail_unknown,
    fail_cleanFailed,
    fail_alreadyRunning,
    fail_noUpdateJsonFile,
    fail_noExecutableKeyInUpdateJson,
    fail_noExecutableFound,
    fail_platformNotMatches,
    fail_archNotMatches,
    fail_modificationNotMatches,
};

class TestZipExtractor: public AbstractZipExtractor
{
public:
    TestZipExtractor(ExpectedPrepareOutcome expectedOutcome):
        m_expectedOutcome(expectedOutcome)
    {}

    virtual void extractAsync(
        const QString& /*filePath*/,
        const QString& outputDir,
        ExtractHandler extractHandler) override
    {
        {
            QnMutexLocker lock(&m_mutex);
            ++m_pendingRequests;
        }

        std::thread(
            [this, self = this, extractHandler, outputDir]()
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
                    case ExpectedPrepareOutcome::fail_noUpdateJsonFile:
                    case ExpectedPrepareOutcome::fail_noExecutableKeyInUpdateJson:
                    case ExpectedPrepareOutcome::fail_noExecutableFound:
                    case ExpectedPrepareOutcome::fail_platformNotMatches:
                    case ExpectedPrepareOutcome::fail_archNotMatches:
                    case ExpectedPrepareOutcome::fail_modificationNotMatches:
                        return extractHandler(QnZipExtractor::Error::Ok, outputDir);
                    case ExpectedPrepareOutcome::fail_noFreeSpace:
                        return extractHandler(QnZipExtractor::Error::NoFreeSpace, QString());
                    case ExpectedPrepareOutcome::fail_cleanFailed:
                        NX_ASSERT(false);
                        break;
                    case ExpectedPrepareOutcome::fail_alreadyRunning:
                    {
                        QnMutexLocker lock(&self->m_mutex);
                        while (!m_mayProceed)
                            m_proceedCondition.wait(lock.mutex());
                        return extractHandler(QnZipExtractor::Error::Ok, outputDir);
                    }
                    case ExpectedPrepareOutcome::fail_unknown:
                        NX_ASSERT(false);
                        break;
                    default:
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
        static const QString kValidPlatform = "valid_platform";
        static const QString kValidArch = "valid_arch";
        static const QString KValidModification = "valid_modification";

        static const QString kInValidPlatform = "invalid_platform";
        static const QString kInValidArch = "invalid_arch";
        static const QString KInValidModification = "invalid_modification";

        static const QString kPlatformKey = "platform";
        static const QString kArchKey = "arch";
        static const QString kModificationKey = "modification";
        static const QString kExecutableKey = "executable";

        static const QString kExcutableName = "some.exe";

        auto createSystemInformation =
            [expectedOutcome]()
            {
                switch (expectedOutcome)
                {
                    case ExpectedPrepareOutcome::success:
                    case ExpectedPrepareOutcome::fail_noExecutableFound:
                    case ExpectedPrepareOutcome::fail_platformNotMatches:
                    case ExpectedPrepareOutcome::fail_archNotMatches:
                    case ExpectedPrepareOutcome::fail_modificationNotMatches:
                        return SystemInformation(kValidPlatform, kValidArch, KValidModification);
                    default:
                        return SystemInformation();
                }
            };

        auto createUpdateInformation =
            [expectedOutcome]()
            {
                QVariantMap result;

                switch (expectedOutcome)
                {
                    case ExpectedPrepareOutcome::success:
                    case ExpectedPrepareOutcome::fail_noExecutableFound:
                        result[kPlatformKey] = kValidPlatform;
                        result[kArchKey] = kValidArch;
                        result[kModificationKey] = KValidModification;
                        result[kExecutableKey] = kExcutableName;
                        break;
                    case ExpectedPrepareOutcome::fail_noExecutableKeyInUpdateJson:
                        result[kPlatformKey] = kValidPlatform;
                        result[kArchKey] = kValidArch;
                        result[kModificationKey] = KValidModification;
                        break;
                    case ExpectedPrepareOutcome::fail_platformNotMatches:
                        result[kPlatformKey] = kInValidPlatform;
                        result[kArchKey] = kValidArch;
                        result[kModificationKey] = KValidModification;
                        result[kExecutableKey] = kExcutableName;
                        break;
                    case ExpectedPrepareOutcome::fail_archNotMatches:
                        result[kPlatformKey] = kValidPlatform;
                        result[kArchKey] = kInValidArch;
                        result[kModificationKey] = KValidModification;
                        result[kExecutableKey] = kExcutableName;
                        break;
                    case ExpectedPrepareOutcome::fail_modificationNotMatches:
                        result[kPlatformKey] = kValidPlatform;
                        result[kArchKey] = kValidArch;
                        result[kModificationKey] = KInValidModification;
                        result[kExecutableKey] = kExcutableName;
                        break;
                    default:
                        break;
                }

                return result;
            };

        auto prepareExtractorCalls =
            [this, expectedOutcome,
            createUpdateInformation, createSystemInformation](int cleanInstallCount)
            {
                EXPECT_CALL(m_updates2Installer, dataDirectoryPath())
                    .Times(1)
                    .WillOnce(Return(kDataDirPath));
                EXPECT_CALL(m_updates2Installer, cleanInstallerDirectory())
                    .Times(cleanInstallCount)
                    .WillRepeatedly(Return(true));
                EXPECT_CALL(m_updates2Installer, createZipExtractor())
                    .Times(1)
                    .WillOnce(Return(m_extractorRef));

                if (expectedOutcome == ExpectedPrepareOutcome::success
                    || expectedOutcome == ExpectedPrepareOutcome::fail_noExecutableFound
                    || expectedOutcome == ExpectedPrepareOutcome::fail_platformNotMatches
                    || expectedOutcome == ExpectedPrepareOutcome::fail_archNotMatches
                    || expectedOutcome == ExpectedPrepareOutcome::fail_modificationNotMatches)
                {
                    EXPECT_CALL(m_updates2Installer, updateInformation(_))
                        .Times(1)
                        .WillOnce(Return(createUpdateInformation()));

                    if (expectedOutcome != ExpectedPrepareOutcome::fail_noExecutableFound)
                    {
                        EXPECT_CALL(m_updates2Installer, systemInformation())
                            .Times(1)
                            .WillOnce(Return(createSystemInformation()));
                    }

                    EXPECT_CALL(m_updates2Installer, checkExecutable(_))
                        .Times(1)
                        .WillOnce(Return(
                            expectedOutcome != ExpectedPrepareOutcome::fail_noExecutableFound));
                }

                if (expectedOutcome == ExpectedPrepareOutcome::fail_noExecutableKeyInUpdateJson)
                {
                    EXPECT_CALL(m_updates2Installer, updateInformation(_))
                        .Times(1)
                        .WillOnce(Return(createUpdateInformation()));
                }
            };

        if (!m_extractorRef)
            m_extractorRef = std::make_shared<TestZipExtractor>(expectedOutcome);

        switch (expectedOutcome)
        {
            case ExpectedPrepareOutcome::success:
                prepareExtractorCalls(1);
                break;
            case ExpectedPrepareOutcome::fail_noFreeSpace:
                prepareExtractorCalls(2);
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
            case ExpectedPrepareOutcome::fail_noUpdateJsonFile:
                prepareExtractorCalls(1);
                EXPECT_CALL(m_updates2Installer, updateInformation(_))
                    .Times(1)
                    .WillOnce(Return(createUpdateInformation()));
                break;
            case ExpectedPrepareOutcome::fail_noExecutableKeyInUpdateJson:
            case ExpectedPrepareOutcome::fail_modificationNotMatches:
            case ExpectedPrepareOutcome::fail_archNotMatches:
            case ExpectedPrepareOutcome::fail_platformNotMatches:
            case ExpectedPrepareOutcome::fail_noExecutableFound:
                prepareExtractorCalls(1);
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

TEST_F(Updates2Installer, prepareFailed_noJsonFileOrCorrupted)
{
    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::fail_noUpdateJsonFile);
    thenPrepareShouldEndWith(PrepareResult::updateContentsError);
}

TEST_F(Updates2Installer, prepareFailed_noExecutableField)
{
    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::fail_noExecutableKeyInUpdateJson);
    thenPrepareShouldEndWith(PrepareResult::updateContentsError);
}

TEST_F(Updates2Installer, prepareFailed_checkExecutableFailed)
{
    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::fail_noExecutableFound);
    thenPrepareShouldEndWith(PrepareResult::updateContentsError);
}

TEST_F(Updates2Installer, prepareFailed_platformNotMatches)
{
    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::fail_platformNotMatches);
    thenPrepareShouldEndWith(PrepareResult::updateContentsError);
}

TEST_F(Updates2Installer, prepareFailed_archNotMatches)
{
    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::fail_archNotMatches);
    thenPrepareShouldEndWith(PrepareResult::updateContentsError);
}

TEST_F(Updates2Installer, prepareFailed_modificationNotMatches)
{
    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::fail_modificationNotMatches);
    thenPrepareShouldEndWith(PrepareResult::updateContentsError);
}

} // namespace test
} // namespace detail
} // namespace installer
} // namespace update
} // namespace nx
