#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/mediaserver/updates2/updates2_manager.h>
#include <utils/common/synctime.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/mediaserver/updates2/detail/update_request_data_factory.h>
#include <nx/update/info/abstract_update_registry.h>

namespace nx {
namespace mediaserver {
namespace updates2 {
namespace detail {
namespace test {

class TestUpdateRegistry: public update::info::AbstractUpdateRegistry
{
public:
    MOCK_CONST_METHOD2(
        findUpdateFile,
        update::info::ResultCode(
            const update::info::UpdateFileRequestData& updateFileRequestData,
            update::info::FileData* outFileData));

    MOCK_CONST_METHOD2(
        latestUpdate,
        update::info::ResultCode(
            const update::info::UpdateRequestData& updateRequestData,
            QnSoftwareVersion* outSoftwareVersion));

    MOCK_CONST_METHOD0(alternativeServers, QList<QString>());
    MOCK_CONST_METHOD0(toByteArray, QByteArray());
    MOCK_METHOD1(fromByteArray, bool(const QByteArray& rawData));
    MOCK_CONST_METHOD1(equals, bool(AbstractUpdateRegistry* other));
};

class TestUpdates2Manager: public Updates2ManagerBase
{
public:
    void setStatus(const detail::Updates2StatusDataEx& status)
    {
        m_currentStatus = status;
    }

    void setGlobalRegistryFactoryFunc(
        std::function<update::info::AbstractUpdateRegistryPtr()> globalRegistryFactoryFunc)
    {
        m_globalRegistryFactoryFunc = globalRegistryFactoryFunc;
    }

    void setRemoteRegistryFactoryFunc(
        std::function<update::info::AbstractUpdateRegistryPtr()> remoteRegistryFactoryFunc)
    {
        m_remoteRegistryFactoryFunc = remoteRegistryFactoryFunc;
    }

    void waitForRemoteUpdate()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_remoteUpdateFinished)
            m_condition.wait(lock.mutex());
    }

    detail::Updates2StatusDataEx fileWrittenData() const
    {
        return m_fileWrittenData;
    }

    MOCK_METHOD0(refreshTimeout, qint64());
    MOCK_METHOD0(loadStatusFromFile, void());
    MOCK_METHOD0(connectToSignals, void());

    virtual update::info::AbstractUpdateRegistryPtr getGlobalRegistry() override
    {
        return m_globalRegistryFactoryFunc();
    }

    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() override
    {
        return m_remoteRegistryFactoryFunc();
    }

    MOCK_METHOD0(moduleGuid, QnUuid());
    MOCK_METHOD1(updateGlobalRegistry, void(const QByteArray& serializedRegistry));

    virtual void writeStatusToFile(const detail::Updates2StatusDataEx& statusData) override
    {
        m_fileWrittenData = statusData;
    }

    MOCK_METHOD0(downloader, vms::common::p2p::downloader::AbstractDownloader*());

    virtual void remoteUpdateCompleted() override
    {
        QnMutexLocker lock(&m_mutex);
        m_remoteUpdateFinished = true;
        m_condition.wakeOne();
    }
private:
    std::function<update::info::AbstractUpdateRegistryPtr()> m_globalRegistryFactoryFunc;
    std::function<update::info::AbstractUpdateRegistryPtr()> m_remoteRegistryFactoryFunc;
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    bool m_remoteUpdateFinished = false;
    detail::Updates2StatusDataEx m_fileWrittenData;
};

using namespace ::testing;

class Updates2Manager: public ::testing::Test
{
protected:
    const static QString kUpdatesUrl;
    const static QString kCloudHost;
    const static QString kCustomization;
    const static QnSoftwareVersion kVersion;
    const static QString kPlatform;
    const static QString kArch;
    const static QString kModification;

    const static QString kFileName;
    const static QString kFileUrl;
    const static int kFileSize;
    const static QByteArray kFileMd5;

    virtual void SetUp() override
    {
        UpdateFileRequestDataFactory::setFactoryFunc(
            []()
            {
                return update::info::UpdateFileRequestData(
                    kCloudHost, kCustomization, kVersion,
                    update::info::OsVersion(kPlatform, kArch, kModification));
            });
    }

    void givenFileState(api::Updates2StatusData::StatusCode state)
    {
        EXPECT_CALL(m_testUpdates2Manager, loadStatusFromFile())
            .Times(1)
            .WillOnce(InvokeWithoutArgs(
                [this, state]()
                {
                    m_testUpdates2Manager.setStatus(detail::Updates2StatusDataEx(
                        1, thisId(), state));
                }));
    }

    void givenGlobalRegistryFactoryFunc(
        std::function<update::info::AbstractUpdateRegistryPtr()> globalRegistryFactoryFunc)
    {
        m_testUpdates2Manager.setGlobalRegistryFactoryFunc(globalRegistryFactoryFunc);
    }

    void givenRemoteRegistryFactoryFunc(
        std::function<update::info::AbstractUpdateRegistryPtr()> remoteRegistryFactoryFunc)
    {
        m_testUpdates2Manager.setRemoteRegistryFactoryFunc(remoteRegistryFactoryFunc);
    }

    void thenStateAfterStartShouldBe(
        api::Updates2StatusData::StatusCode state,
        const QString& message = "")
    {
        ASSERT_EQ(state, m_testUpdates2Manager.status().state);
        ASSERT_TRUE(m_testUpdates2Manager.status().message.toLower().contains(message));
        expectingStateWrittenToFile(state);
    }

    void whenServerStarted()
    {
        EXPECT_CALL(m_testUpdates2Manager, connectToSignals())
            .Times(1);

        EXPECT_CALL(m_testUpdates2Manager, refreshTimeout())
            .Times(AtLeast(1)).WillRepeatedly(Return(1000));

        EXPECT_CALL(m_testUpdates2Manager, moduleGuid())
            .Times(AtLeast(0)).WillRepeatedly(Return(thisId()));

        m_testUpdates2Manager.atServerStart();
    }

    void whenRemoteUpdateDone()
    {
        m_testUpdates2Manager.waitForRemoteUpdate();
    }

    update::info::AbstractUpdateRegistryPtr createUpdateRegistry(
        const QnSoftwareVersion* version,
        bool hasUpdate,
        bool expectToByteArrayWillBeCalled)
    {
        auto updateRegistry = std::make_unique<TestUpdateRegistry>();

        if (hasUpdate)
        {
            NX_ASSERT(version);

            EXPECT_CALL(*updateRegistry, latestUpdate(_, NotNull()))
                .Times(AtLeast(1))
                .WillRepeatedly(
                    DoAll(SetArgPointee<1>(*version), Return(update::info::ResultCode::ok)));

            update::info::FileData fileData(kFileName, kFileUrl, kFileSize, kFileMd5);
            EXPECT_CALL(*updateRegistry, findUpdateFile(_, NotNull()))
                .Times(AtLeast(1))
                .WillRepeatedly(
                    DoAll(SetArgPointee<1>(fileData), Return(update::info::ResultCode::ok)));
        }
        else
        {
            EXPECT_CALL(*updateRegistry, latestUpdate(_, NotNull()))
                .Times(AtLeast(1))
                .WillRepeatedly(Return(update::info::ResultCode::noData));
        }

        if (expectToByteArrayWillBeCalled)
        {
            EXPECT_CALL(*updateRegistry, toByteArray())
                .Times(AtLeast(1))
                .WillRepeatedly(Return(QByteArray()));
        }

        return std::move(updateRegistry);
    }

    void expectingUpdateGlobalRegistryWillBeCalled()
    {
        EXPECT_CALL(m_testUpdates2Manager, updateGlobalRegistry(_)).Times(1);
    }

private:
    QnSyncTime m_syncTimeInstance;
    TestUpdates2Manager m_testUpdates2Manager;

    static QnUuid thisId()
    {
        const static QnUuid result = QnUuid::createUuid();
        return result;
    }

    void expectingStateWrittenToFile(api::Updates2StatusData::StatusCode state)
    {
        ASSERT_EQ(state, m_testUpdates2Manager.fileWrittenData().state);
    }
};

const QString Updates2Manager::kUpdatesUrl = "test.url";
const QString Updates2Manager::kCloudHost = "test.cloud.host";
const QString Updates2Manager::kCustomization = "test.customization";
const QnSoftwareVersion Updates2Manager::kVersion = QnSoftwareVersion("1.0.0.1");
const QString Updates2Manager::kPlatform = "test.platform";
const QString Updates2Manager::kArch = "test.arch";
const QString Updates2Manager::kModification = "test.modification";

const QString Updates2Manager::kFileName = "test.file.name";
const QString Updates2Manager::kFileUrl = "test.file.url";
const int Updates2Manager::kFileSize = 42;
const QByteArray Updates2Manager::kFileMd5 = "test.file.md5";

TEST_F(Updates2Manager, NotAvailableStatusCheck)
{
    givenFileState(api::Updates2StatusData::StatusCode::notAvailable);
    givenGlobalRegistryFactoryFunc([](){ return update::info::AbstractUpdateRegistryPtr(); });
    givenRemoteRegistryFactoryFunc([](){ return update::info::AbstractUpdateRegistryPtr(); });
    whenServerStarted();
    whenRemoteUpdateDone();
    thenStateAfterStartShouldBe(api::Updates2StatusData::StatusCode::error, "fail");
}

TEST_F(Updates2Manager, FoundGlobalUpdateRegistry_noNewVersion)
{
    givenFileState(api::Updates2StatusData::StatusCode::notAvailable);
    givenGlobalRegistryFactoryFunc(
        [this]()
        {
            return createUpdateRegistry(
                /*version*/ nullptr,
                /*hasUpdate*/ false,
                /*expectToByteArrayWillBeCalled*/ false);
        });
    givenRemoteRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
    whenServerStarted();
    whenRemoteUpdateDone();
    thenStateAfterStartShouldBe(api::Updates2StatusData::StatusCode::notAvailable);
}

TEST_F(Updates2Manager, FoundGlobalUpdateRegistry_newVersion)
{
    const auto newVersion = QnSoftwareVersion("1.0.0.2");
    givenFileState(api::Updates2StatusData::StatusCode::notAvailable);
    givenGlobalRegistryFactoryFunc(
        [this, &newVersion]()
        {
            return createUpdateRegistry(
                &newVersion,
                /*hasUpdate*/ true,
                /*expectToByteArrayWillBeCalled*/ false);
        });
    givenRemoteRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
    whenServerStarted();
    whenRemoteUpdateDone();
    thenStateAfterStartShouldBe(
        api::Updates2StatusData::StatusCode::available, newVersion.toString());
}

TEST_F(Updates2Manager, FoundRemoteUpdateRegistry_noNewVersion)
{
    givenFileState(api::Updates2StatusData::StatusCode::notAvailable);
    givenGlobalRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
    givenRemoteRegistryFactoryFunc(
        [this]()
        {
            return createUpdateRegistry(
                /*version*/ nullptr,
                /*hasUpdate*/ false,
                /*expectToByteArrayWillBeCalled*/ true);
        });
    expectingUpdateGlobalRegistryWillBeCalled();
    whenServerStarted();
    whenRemoteUpdateDone();
    thenStateAfterStartShouldBe(api::Updates2StatusData::StatusCode::notAvailable);
}


} // namespace test
} // namespace detail
} // namespace updates2
} // namespace mediaserver
} // namespace nx
