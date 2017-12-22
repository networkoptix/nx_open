#include <functional>
#include <gtest/gtest.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/random.h>
#include <nx/update/info/async_update_checker.h>
#include <nx/update/info/detail/data_provider/raw_data_provider_factory.h>
#include <nx/update/info/impl/update_registry_factory.h>
#include <nx/update/info/detail/data_provider/test_support/impl/async_json_provider_mockup.h>

namespace nx {
namespace update {
namespace info {
namespace test {

static const QString kBaseUrl = "http://updates.networkoptix.com";

class AsyncUpdateChecker: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
    }

    virtual void TearDown() override
    {
        using namespace detail::data_provider;
        RawDataProviderFactory::setFactoryFunction(nullptr);
    }

    void whenMockupDataProviderHasBeenSetUp() const
    {
        using namespace detail::data_provider;

        RawDataProviderFactory::setFactoryFunction(
            [](const QString& s, AbstractAsyncRawDataProviderHandler* handler)
            {
                return std::make_unique<test_support::AsyncJsonProviderMockup>(handler);
            });
    }

    void whenAsyncCheckRequestHasBeenIssued()
    {
        using namespace std::placeholders;
        m_asyncUpdateChecker.check(
            std::bind(&AsyncUpdateChecker::onCheckCompleted, this, _1, _2),
            kBaseUrl);
    }

    void thenCorrectUpdateRegistryShouldBeReturned()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_done)
            m_condition.wait(lock.mutex());

        ASSERT_EQ(ResultCode::ok, m_resultCode);
        ASSERT_TRUE(m_updateRegistry);
        assertUpdateRegistryContent();
    }

private:
    info::AsyncUpdateChecker m_asyncUpdateChecker;
    AbstractUpdateRegistryPtr m_updateRegistry;
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    bool m_done = false;
    ResultCode m_resultCode = ResultCode::noData;

    void onCheckCompleted(ResultCode resultCode, AbstractUpdateRegistryPtr updateRegistry)
    {
        QnMutexLocker lock(&m_mutex);
        m_resultCode = resultCode;
        m_done = true;
        m_updateRegistry = std::move(updateRegistry);
        m_condition.wakeOne();
    }

    void assertUpdateRegistryContent() const
    {
        assertAlternativeServer();
        assertFileDataContent();
        assertUpdateDataContent();
        assertSerializability();
    }

    void assertAlternativeServer() const
    {
        ASSERT_EQ(1, m_updateRegistry->alternativeServers().size());
        ASSERT_FALSE(m_updateRegistry->alternativeServers()[0].isEmpty());
    }


    void assertUpdateDataContent() const
    {
        QnSoftwareVersion softwareVersion;
        ResultCode resultCode = m_updateRegistry->latestUpdate(
            UpdateRequestData("nxvms.com", "default", QnSoftwareVersion(2, 0, 0, 0)),
            &softwareVersion);
        assertLatestUpdateResult(
            resultCode,
            QnSoftwareVersion("3.1.0.16975"),
            softwareVersion,
            false);

        resultCode = m_updateRegistry->latestUpdate(
            UpdateRequestData("nxvms.com", "default", QnSoftwareVersion(4, 0, 0, 0)),
            &softwareVersion);
        assertLatestUpdateResult(
            resultCode,
            QnSoftwareVersion("3.1.0.16975"),
            softwareVersion,
            true);

        resultCode = m_updateRegistry->latestUpdate(
            UpdateRequestData("nxvms.com", "default", QnSoftwareVersion(3, 2, 0, 0)),
            &softwareVersion);
        assertLatestUpdateResult(
            resultCode,
            QnSoftwareVersion("3.1.0.16975"),
            softwareVersion,
            true);

        resultCode = m_updateRegistry->latestUpdate(
            UpdateRequestData("invalid.host.com", "default", QnSoftwareVersion(2, 1, 0, 0)),
            &softwareVersion);
        assertLatestUpdateResult(
            resultCode,
            QnSoftwareVersion("3.1.0.16975"),
            softwareVersion,
            true);

        resultCode = m_updateRegistry->latestUpdate(
            UpdateRequestData("qcloud.vista-cctv.com", "vista", QnSoftwareVersion(2, 1, 0, 0)),
            &softwareVersion);
        assertLatestUpdateResult(
            resultCode,
            QnSoftwareVersion("3.1.0.16975"),
            softwareVersion,
            false);

        resultCode = m_updateRegistry->latestUpdate(
            UpdateRequestData("tricom.cloud-demo.hdw.mx", "tricom", QnSoftwareVersion(2, 1, 0, 0)),
            &softwareVersion);
        assertLatestUpdateResult(
            resultCode,
            QnSoftwareVersion("3.0.0.14532"),
            softwareVersion,
            false);
    }

    void assertLatestUpdateResult(
        const ResultCode resultCode,
        const QnSoftwareVersion& expectedVersion,
        const QnSoftwareVersion& actualVersion,
        const bool shouldFail) const
    {
        if (shouldFail)
        {
            ASSERT_NE(ResultCode::ok, resultCode);
            return;
        }

        ASSERT_EQ(ResultCode::ok, resultCode);
        ASSERT_EQ(expectedVersion, actualVersion);
    }

    void assertFileDataContent() const
    {
        FileData fileData;
        ResultCode resultCode = m_updateRegistry->findUpdateFile(
            UpdateFileRequestData("nxvms.com", "default", QnSoftwareVersion(2, 0, 0, 0), ubuntuX64()),
            &fileData);
        assertFindResult(resultCode, fileData, false);

        resultCode = m_updateRegistry->findUpdateFile(
            UpdateFileRequestData("nxvms.com", "default", QnSoftwareVersion(10, 0, 0, 0), ubuntuX64()),
            &fileData);
        assertFindResult(resultCode, fileData, true);

        resultCode = m_updateRegistry->findUpdateFile(
            UpdateFileRequestData("not-existing-host", "default", QnSoftwareVersion(2, 0, 0, 0), ubuntuX64()),
            &fileData);
        assertFindResult(resultCode, fileData, true);

        resultCode = m_updateRegistry->findUpdateFile(
            UpdateFileRequestData("nxvms.com", "not-existing-customization", QnSoftwareVersion(2, 0, 0, 0), ubuntuX64()),
            &fileData);
        assertFindResult(resultCode, fileData, true);

        resultCode = m_updateRegistry->findUpdateFile(
            UpdateFileRequestData("qcloud.vista-cctv.com", "vista", QnSoftwareVersion(3, 0, 2, 9872), windowsX64()),
            &fileData);
        assertFindResult(resultCode, fileData, false);

        resultCode = m_updateRegistry->findUpdateFile(
            UpdateFileRequestData("tricom.cloud-demo.hdw.mx", "tricom", QnSoftwareVersion(3, 0, 0, 9872), armRpi()),
            &fileData);
        assertFindResult(resultCode, fileData, false);
    }

    void assertFindResult(ResultCode resultCode, const FileData& fileData, bool shouldFail) const
    {
        if (shouldFail)
        {
            ASSERT_NE(ResultCode::ok, resultCode);
            return;
        }

        ASSERT_EQ(ResultCode::ok, resultCode);
        ASSERT_FALSE(fileData.url.isEmpty());
        ASSERT_FALSE(fileData.file.isEmpty());
        ASSERT_TRUE(fileData.url.contains(kBaseUrl));
        ASSERT_TRUE(fileData.url.contains(fileData.file));
    }

    void assertSerializability() const
    {
        const auto rawData = m_updateRegistry->toByteArray();
        auto newUpdateRegistry = impl::UpdateRegistryFactory::create();
        ASSERT_TRUE(newUpdateRegistry->fromByteArray(rawData));
        ASSERT_TRUE(m_updateRegistry->equals(newUpdateRegistry.get()));
    }
};

TEST_F(AsyncUpdateChecker, CorrectUpdateRegistryProvided)
{
    whenMockupDataProviderHasBeenSetUp();
    whenAsyncCheckRequestHasBeenIssued();
    thenCorrectUpdateRegistryShouldBeReturned();
}

} // namespace test
} // namespace info
} // namespace update
} // namespace nx
