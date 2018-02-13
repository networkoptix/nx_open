#include <nx/update/installer/detail/updates2_installer_base.h>
#include <nx/utils/std/future.h>
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

class TestZipExtractor: public AbstractZipExtractor
{
public:
    MOCK_METHOD3(
        extractAsync,
        void(const QString& filePath, const QString& outputDir, ExtractHandler extractHandler));

    MOCK_METHOD0(stop, void());
};

class Updates2Installer: public ::testing::Test
{
protected:
    enum class ExpectedPrepareOutcome
    {
        success
    };

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
//            EXPECT_CALL(m_updates2Installer, createZipExtractor())
//                .Times(1)
//                .WillRepeatedly(Return());
            // #TODO #akulikov uncomment snippet above and implement
            break;
        }
    }
};

TEST_F(Updates2Installer, prepareSuccessful)
{
//    whenPrepareRequestIsIssued(ExpectedPrepareOutcome::success);
//    thenPrepareShouldEndWith(PrepareResult::ok);
    // #TODO #akulikov uncomment snippet above and implement
}

} // namespace test
} // namespace detail
} // namespace update
} // namespace nx
