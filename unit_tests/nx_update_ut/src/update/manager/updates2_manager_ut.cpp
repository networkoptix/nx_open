#include <nx/update/manager/detail/update_request_data_factory.h>
#include <nx/update/installer/detail/abstract_updates2_installer.h>
#include <nx/update/info/abstract_update_registry.h>
#include <nx/update/info/manual_file_data.h>
#include <nx/update/manager/detail/updates2_manager_base.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/wait_condition.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>

namespace nx {
namespace update {
namespace manager {
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
    MOCK_METHOD2(addFileData, void(const update::info::UpdateFileRequestData&,
        const update::info::FileData&));
    MOCK_METHOD1(merge, void(AbstractUpdateRegistry* other));
    MOCK_METHOD1(addFileData, void(const info::ManualFileData& fileData));
};

using namespace vms::common::p2p;

class TestDownloader: downloader::AbstractDownloader
{
public:
    TestDownloader(): downloader::AbstractDownloader(nullptr) {}

    MOCK_CONST_METHOD0(files, QStringList());
    MOCK_CONST_METHOD1(filePath, QString(const QString& fileName));
    MOCK_CONST_METHOD1(fileInformation, downloader::FileInformation(const QString& fileName));
    MOCK_METHOD1(
        addFile, downloader::ResultCode(const downloader::FileInformation& fileInformation));
    MOCK_METHOD3(
        updateFileInformation,
        downloader::ResultCode(const QString& fileName, int size, const QByteArray& md5));
    MOCK_METHOD3(
        writeFileChunk,
        downloader::ResultCode(const QString& fileName, int chunkIndex, const QByteArray& buffer));
    MOCK_METHOD3(
        readFileChunk,
        downloader::ResultCode(const QString& fileName, int chunkIndex,QByteArray& buffer));
    MOCK_METHOD2(
        deleteFile,
        downloader::ResultCode(const QString& fileName, bool deleteData));
    MOCK_METHOD1(
        getChunkChecksums, QVector<QByteArray>(const QString& fileName));
};

enum class PrepareExpectedOutcome
{
    success,
    fail_noFreeSpace
};

const static QString kFileName = "test.file.name";

class TestInstaller: public installer::detail::AbstractUpdates2Installer, public QnLongRunnable
{
public:
    virtual void prepareAsync(
        const QString& /*path*/,
        installer::detail::PrepareUpdateCompletionHandler handler) override
    {
        put(handler);
    }

    MOCK_METHOD0(install, bool());

    void setExpectedOutcome(PrepareExpectedOutcome expectedOutcome)
    {
        m_expectedOutcome = expectedOutcome;
    }

    MOCK_METHOD0(stopSync, void());

    ~TestInstaller()
    {
        QnLongRunnable::stop();
    }

private:
    const static int s_timeoutBeforeTaskMs = 300;
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    QQueue<installer::detail::PrepareUpdateCompletionHandler> m_queue;
    PrepareExpectedOutcome m_expectedOutcome = PrepareExpectedOutcome::success;

    virtual void run() override
    {
        QnMutexLocker lock(&m_mutex);
        while (!needToStop())
        {
            while (m_queue.isEmpty() && !needToStop())
                m_condition.wait(lock.mutex());

            if (needToStop())
                return;

            const auto handler = m_queue.front();
            m_queue.pop_front();

            lock.unlock();
            QThread::msleep(s_timeoutBeforeTaskMs);

            switch (m_expectedOutcome)
            {
                case PrepareExpectedOutcome::success:
                    handler(installer::detail::PrepareResult::ok);
                    break;
                case PrepareExpectedOutcome::fail_noFreeSpace:
                    handler(installer::detail::PrepareResult::noFreeSpace);
                    break;
            }

            lock.relock();
        }
    }

    virtual void pleaseStop() override
    {
        QnMutexLocker lock(&m_mutex);
        QnLongRunnable::pleaseStop();
        m_condition.wakeOne();
    }

    void put(installer::detail::PrepareUpdateCompletionHandler taskFunc)
    {
        QnMutexLocker lock(&m_mutex);
        m_queue.push_back(taskFunc);
        m_condition.wakeOne();
    }
};

class TestUpdates2Manager: public Updates2ManagerBase
{
public:
    void setStatus(const detail::Updates2StatusDataEx& status)
    {
        m_currentStatus.clone(status);
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
            m_remoteUpdateCondition.wait(lock.mutex());
        m_remoteUpdateFinished = false;
    }

    void emitDownloadFinishedSignal(const QString& fileName)
    {
        onDownloadFinished(fileName);
    }

    void emitDownloadFailedSignal(const QString& fileName)
    {
        onDownloadFailed(fileName);
    }

    void emitCunkDownloadFailedSignal(const QString& fileName)
    {
        onChunkDownloadFailed(fileName);
    }

    const detail::Updates2StatusDataEx& fileWrittenData() const
    {
        return m_fileWrittenData;
    }

    void setNeedToWaitForgetRemoteRegistry(bool value)
    {
        QnMutexLocker lock(&m_mutex);
        m_getRemoteRegistryFinished = !value;
        m_getRemoteRegistryCondition.wakeOne();
    }

    MOCK_CONST_METHOD0(refreshTimeout, qint64());
    MOCK_CONST_METHOD0(filePath, QString());
    MOCK_METHOD0(loadStatusFromFile, void());
    MOCK_METHOD0(connectToSignals, void());

    virtual update::info::AbstractUpdateRegistryPtr getGlobalRegistry() override
    {
        return m_globalRegistryFactoryFunc();
    }

    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() override
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_getRemoteRegistryFinished)
            m_getRemoteRegistryCondition.wait(lock.mutex());

        return m_remoteRegistryFactoryFunc();
    }

    MOCK_CONST_METHOD0(moduleGuid, QnUuid());
    MOCK_METHOD1(updateGlobalRegistry, void(const QByteArray& serializedRegistry));

    virtual void writeStatusToFile(const detail::Updates2StatusDataEx& statusData) override
    {
        m_fileWrittenData.clone(statusData);
    }

    MOCK_METHOD0(downloader, vms::common::p2p::downloader::AbstractDownloader*());
    MOCK_METHOD0(installer, installer::detail::AbstractUpdates2Installer*());

    virtual void remoteUpdateCompleted() override
    {
        QnMutexLocker lock(&m_mutex);
        m_remoteUpdateFinished = true;
        m_remoteUpdateCondition.wakeOne();
    }

    MOCK_CONST_METHOD0(isClient, bool());
    MOCK_CONST_METHOD0(peerId, QnUuid());

private:
    std::function<update::info::AbstractUpdateRegistryPtr()> m_globalRegistryFactoryFunc;
    std::function<update::info::AbstractUpdateRegistryPtr()> m_remoteRegistryFactoryFunc;
    QnMutex m_mutex;
    QnWaitCondition m_remoteUpdateCondition;
    bool m_remoteUpdateFinished = false;
    detail::Updates2StatusDataEx m_fileWrittenData;
    QnWaitCondition m_getRemoteRegistryCondition;
    bool m_getRemoteRegistryFinished = true;
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

    const static QString kFileUrl;
    const static int kFileSize;
    const static QByteArray kFileMd5;

    enum class DownloadExpectedOutcome
    {
        success_fileNotExists,
        success_fileAlreadyExists,
        success_fileAlreadyDownloaded,
        fail_addFileFailed,
        fail_downloadFailed,
        fail_wrongState,
    };

    virtual void SetUp() override
    {
        detail::UpdateFileRequestDataFactory::setFactoryFunc(
            []()
            {
                return update::info::UpdateFileRequestData(kCloudHost, kCustomization, kVersion,
                    update::info::OsVersion(kPlatform, kArch, kModification), false);
            });
        m_testInstaller.start();
    }

    virtual void TearDown() override
    {
        m_testUpdates2Manager.stopAsyncTasks();
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

    void givenAvailableRemoteUpdate()
    {
        const auto newVersion = QnSoftwareVersion("1.0.0.2");
        givenFileState(api::Updates2StatusData::StatusCode::notAvailable);
        givenGlobalRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
        givenRemoteRegistryFactoryFunc(
            [this, &newVersion]()
            {
                return createUpdateRegistry(
                    /*version*/ &newVersion,
                    /*hasUpdate*/ true,
                    /*expectFindUpdateFileWillBeCalled*/ true,
                    /*expectToByteArrayWillBeCalled*/ true);
            });
        expectingUpdateGlobalRegistryWillBeCalled();
        whenServerStarted();
        whenRemoteUpdateDone();
        thenStateShouldBe(api::Updates2StatusData::StatusCode::available);
    }

    void givenInstallerInstallWithTheOutcome(PrepareExpectedOutcome outcome)
    {
        m_testInstaller.setExpectedOutcome(outcome);
    }

    void thenStateShouldBe(
        api::Updates2StatusData::StatusCode state,
        const QString& message = "")
    {
        ASSERT_EQ(state, m_testUpdates2Manager.status().state);
        ASSERT_TRUE(m_testUpdates2Manager.status().message.toLower().contains(message));
        expectingStateWrittenToFile(state);
    }

    void thenStateShouldBeAtLast(api::Updates2StatusData::StatusCode state)
    {
        while (m_testUpdates2Manager.status().state != state)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
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

    void whenNeedToWaitForRemoteRegistryCompletion()
    {
        m_testUpdates2Manager.setNeedToWaitForgetRemoteRegistry(true);
    }

    void whenRemoteUpdateDone()
    {
        m_testUpdates2Manager.setNeedToWaitForgetRemoteRegistry(false);
        m_testUpdates2Manager.waitForRemoteUpdate();
    }

    void whenDownloadRequestIssued(DownloadExpectedOutcome expectedOutcome)
    {
        prepareDownloadExpectations(expectedOutcome);
        m_testUpdates2Manager.download();
    }

    void whenChunkFailedSignalReceived()
    {
        m_testUpdates2Manager.emitCunkDownloadFailedSignal(kFileName);
    }

    void prepareDownloadExpectations(DownloadExpectedOutcome expectedOutcome)
    {
        if (expectedOutcome != DownloadExpectedOutcome::fail_wrongState)
        {
            EXPECT_CALL(m_testUpdates2Manager, downloader())
                .Times(AtLeast(1))
                .WillRepeatedly(Return((downloader::AbstractDownloader*)&m_testDownloader));
        }

        downloader::FileInformation fileInformation(kFileName);
        fileInformation.url = kFileUrl;
        fileInformation.md5 = QByteArray::fromHex(kFileMd5.toBase64());
        fileInformation.size = kFileSize;
        fileInformation.peerPolicy =
            downloader::FileInformation::PeerSelectionPolicy::byPlatform;

        auto setSuccessfulExpectations =
            [this, &fileInformation](downloader::ResultCode downloadResult)
            {
                EXPECT_CALL(m_testDownloader, deleteFile(fileInformation.name, true))
                    .Times(1).WillOnce(Return(downloader::ResultCode::ok));
                EXPECT_CALL(m_testDownloader, addFile(fileInformation))
                    .Times(1).WillOnce(Return(downloadResult));
                fileInformation.status = downloader::FileInformation::Status::downloaded;
                EXPECT_CALL(m_testDownloader, fileInformation(kFileName))
                    .Times(1).WillOnce(Return(fileInformation));
                EXPECT_CALL(m_testDownloader, filePath(_))
                    .Times(1).WillOnce(Return(kFileName));
                EXPECT_CALL(m_testUpdates2Manager, installer())
                    .Times(1)
                    .WillOnce(Return(&m_testInstaller));
            };


        switch (expectedOutcome)
        {
            case DownloadExpectedOutcome::success_fileNotExists:
                setSuccessfulExpectations(downloader::ResultCode::ok);
                break;
            case DownloadExpectedOutcome::fail_downloadFailed:
                EXPECT_CALL(m_testDownloader, addFile(fileInformation))
                    .Times(1).WillOnce(Return(downloader::ResultCode::ok));
                EXPECT_CALL(m_testDownloader, deleteFile(kFileName, _))
                    .Times(2).WillRepeatedly(Return(downloader::ResultCode::ok));
                fileInformation.status = downloader::FileInformation::Status::corrupted;
                EXPECT_CALL(m_testDownloader, fileInformation(kFileName))
                    .Times(1).WillOnce(Return(fileInformation));
                break;
            case DownloadExpectedOutcome::fail_addFileFailed:
                EXPECT_CALL(m_testDownloader, deleteFile(fileInformation.name, true))
                    .Times(1).WillOnce(Return(downloader::ResultCode::ok));
                EXPECT_CALL(m_testDownloader, addFile(fileInformation))
                    .Times(1).WillOnce(Return(downloader::ResultCode::ioError));
                break;
            case DownloadExpectedOutcome::success_fileAlreadyDownloaded:
            case DownloadExpectedOutcome::success_fileAlreadyExists:
                // File should has been deleted by deleteFile() call so addFile should return ok,
                // not fileAlreadyExists
                setSuccessfulExpectations(downloader::ResultCode::ok);
                break;
            case DownloadExpectedOutcome::fail_wrongState:
                break;
        }
    }

    void whenDownloadFinishedSuccessfully()
    {
        m_testUpdates2Manager.emitDownloadFinishedSignal(kFileName);
    }

    void whenDownloadFinishedWithFailure()
    {
        m_testUpdates2Manager.emitDownloadFailedSignal(kFileName);
    }

    update::info::AbstractUpdateRegistryPtr createUpdateRegistry(
        const QnSoftwareVersion* version,
        bool hasUpdate,
        bool expectFindUpdateFileWillBeCalled,
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

            if (expectFindUpdateFileWillBeCalled)
            {
                update::info::FileData fileData(kFileName, kFileUrl, kFileSize, kFileMd5);
                EXPECT_CALL(*updateRegistry, findUpdateFile(_, NotNull()))
                    .Times(AtLeast(1))
                    .WillRepeatedly(
                        DoAll(SetArgPointee<1>(fileData), Return(update::info::ResultCode::ok)));
            }
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
    TestDownloader m_testDownloader;
    TestUpdates2Manager m_testUpdates2Manager;
    TestInstaller m_testInstaller;

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
    thenStateShouldBe(api::Updates2StatusData::StatusCode::error, "fail");
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
                /*expectFindUpdateFileWillBeCalled*/ true,
                /*expectToByteArrayWillBeCalled*/ false);
        });
    givenRemoteRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
    whenServerStarted();
    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::notAvailable);
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
                /*expectFindUpdateFileWillBeCalled*/ true,
                /*expectToByteArrayWillBeCalled*/ false);
        });
    givenRemoteRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
    whenServerStarted();
    whenRemoteUpdateDone();
    thenStateShouldBe(
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
                /*expectFindUpdateFileWillBeCalled*/ true,
                /*expectToByteArrayWillBeCalled*/ true);
        });
    expectingUpdateGlobalRegistryWillBeCalled();
    whenServerStarted();
    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::notAvailable);
}

TEST_F(Updates2Manager, FoundRemoteUpdateRegistry_newVersion)
{
    const auto newVersion = QnSoftwareVersion("1.0.0.2");
    givenFileState(api::Updates2StatusData::StatusCode::notAvailable);
    givenGlobalRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
    givenRemoteRegistryFactoryFunc(
        [this, &newVersion]()
        {
            return createUpdateRegistry(
                /*version*/ &newVersion,
                /*hasUpdate*/ true,
                /*expectFindUpdateFileWillBeCalled*/ true,
                /*expectToByteArrayWillBeCalled*/ true);
        });
    expectingUpdateGlobalRegistryWillBeCalled();

    whenServerStarted();
    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, StatusWhileCheckingForUpdate)
{
    const auto newVersion = QnSoftwareVersion("1.0.0.2");
    givenFileState(api::Updates2StatusData::StatusCode::notAvailable);
    givenGlobalRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
    givenRemoteRegistryFactoryFunc(
        [this, &newVersion]()
    {
        return createUpdateRegistry(
            /*version*/ &newVersion,
            /*hasUpdate*/ true,
            /*expectFindUpdateFileWillBeCalled*/ true,
            /*expectToByteArrayWillBeCalled*/ true);
    });
    whenNeedToWaitForRemoteRegistryCompletion();
    expectingUpdateGlobalRegistryWillBeCalled();
    whenServerStarted();
    thenStateShouldBeAtLast(api::Updates2StatusData::StatusCode::checking);
    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, Download_successful)
{
    givenAvailableRemoteUpdate();
    whenDownloadRequestIssued(DownloadExpectedOutcome::success_fileNotExists);
    whenDownloadFinishedSuccessfully();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::preparing);
    thenStateShouldBeAtLast(api::Updates2StatusData::StatusCode::readyToInstall);
}

TEST_F(Updates2Manager, Download_addFile_fail)
{
    givenAvailableRemoteUpdate();
    whenDownloadRequestIssued(DownloadExpectedOutcome::fail_addFileFailed);
    thenStateShouldBe(api::Updates2StatusData::StatusCode::error);

    // now update registry with a new version should already be in global settings
    const auto newVersion = QnSoftwareVersion("1.0.0.2");
    givenGlobalRegistryFactoryFunc(
        [this, &newVersion]()
        {
            return createUpdateRegistry(
                &newVersion,
                /*hasUpdate*/ true,
                /*expectFindUpdateFileWillBeCalled*/ false,
                /*expectToByteArrayWillBeCalled*/ false);
        });

    givenRemoteRegistryFactoryFunc(
        [this, &newVersion]()
        {
            return createUpdateRegistry(
                /*version*/ &newVersion,
                /*hasUpdate*/ true,
                /*expectFindUpdateFileWillBeCalled*/ false,
                /*expectToByteArrayWillBeCalled*/ false);
        });

    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, Download_notAvailableState_failedToGetUpdateInfo)
{
    givenFileState(api::Updates2StatusData::StatusCode::notAvailable);
    givenGlobalRegistryFactoryFunc(
        [this]()
        {
            return createUpdateRegistry(
                /*version*/ nullptr,
                /*hasUpdate*/ false,
                /*expectFindUpdateFileWillBeCalled*/ true,
                /*expectToByteArrayWillBeCalled*/ false);
        });
    givenRemoteRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
    whenServerStarted();
    whenRemoteUpdateDone();
    whenDownloadRequestIssued(DownloadExpectedOutcome::fail_wrongState);
    thenStateShouldBe(api::Updates2StatusData::StatusCode::notAvailable);
}

TEST_F(Updates2Manager, Download_notAvailableState_GotUpdateInfo_noNewVersions)
{
    givenFileState(api::Updates2StatusData::StatusCode::notAvailable);
    givenGlobalRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
    givenRemoteRegistryFactoryFunc([]() { return update::info::AbstractUpdateRegistryPtr(); });
    whenServerStarted();
    whenRemoteUpdateDone();
    whenDownloadRequestIssued(DownloadExpectedOutcome::fail_wrongState);
    thenStateShouldBe(api::Updates2StatusData::StatusCode::error);
}

TEST_F(Updates2Manager, Download_alreadyDownloadingState)
{
    givenAvailableRemoteUpdate();
    whenDownloadRequestIssued(DownloadExpectedOutcome::success_fileNotExists);

    whenDownloadRequestIssued(DownloadExpectedOutcome::fail_wrongState);
    thenStateShouldBe(api::Updates2StatusData::StatusCode::downloading);

    whenDownloadFinishedSuccessfully();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::preparing);
    thenStateShouldBeAtLast(api::Updates2StatusData::StatusCode::readyToInstall);
}

TEST_F(Updates2Manager, Download_failed)
{
    givenAvailableRemoteUpdate();
    whenDownloadRequestIssued(DownloadExpectedOutcome::fail_downloadFailed);
    thenStateShouldBe(api::Updates2StatusData::StatusCode::downloading);

    whenDownloadFinishedWithFailure();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::error);

    // now update registry with a new version should already be in global settings
    const auto newVersion = QnSoftwareVersion("1.0.0.2");
    givenGlobalRegistryFactoryFunc(
        [this, &newVersion]()
        {
            return createUpdateRegistry(
                &newVersion,
                /*hasUpdate*/ true,
                /*expectFindUpdateFileWillBeCalled*/ false,
                /*expectToByteArrayWillBeCalled*/ false);
        });

    givenRemoteRegistryFactoryFunc(
        [this, &newVersion]()
        {
            return createUpdateRegistry(
                /*version*/ &newVersion,
                /*hasUpdate*/ true,
                /*expectFindUpdateFileWillBeCalled*/ false,
                /*expectToByteArrayWillBeCalled*/ false);
        });

    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, Download_chunkFailedAndRecovered)
{
    givenAvailableRemoteUpdate();
    whenDownloadRequestIssued(DownloadExpectedOutcome::success_fileNotExists);
    thenStateShouldBe(api::Updates2StatusData::StatusCode::downloading);

    whenChunkFailedSignalReceived();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::downloading, "problems");

    whenDownloadFinishedSuccessfully();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::preparing);
    thenStateShouldBeAtLast(api::Updates2StatusData::StatusCode::readyToInstall);
}

TEST_F(Updates2Manager, Download_addAlreadyExistingFile)
{
    givenAvailableRemoteUpdate();
    whenDownloadRequestIssued(DownloadExpectedOutcome::success_fileAlreadyExists);

    thenStateShouldBe(api::Updates2StatusData::StatusCode::downloading);

    whenDownloadFinishedSuccessfully();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::preparing);
    thenStateShouldBeAtLast(api::Updates2StatusData::StatusCode::readyToInstall);
}

TEST_F(Updates2Manager, Prepare_successfulAndReadyForInstall)
{
    givenAvailableRemoteUpdate();
    givenInstallerInstallWithTheOutcome(PrepareExpectedOutcome::success);

    whenDownloadRequestIssued(DownloadExpectedOutcome::success_fileNotExists);
    whenDownloadFinishedSuccessfully();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::preparing);
    thenStateShouldBeAtLast(api::Updates2StatusData::StatusCode::readyToInstall);
}

TEST_F(Updates2Manager, Prepare_failedNoFreeSpace)
{
    givenAvailableRemoteUpdate();
    givenInstallerInstallWithTheOutcome(PrepareExpectedOutcome::fail_noFreeSpace);

    whenDownloadRequestIssued(DownloadExpectedOutcome::success_fileNotExists);
    whenDownloadFinishedSuccessfully();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::preparing);
    thenStateShouldBeAtLast(api::Updates2StatusData::StatusCode::error);

    const auto newVersion = QnSoftwareVersion("1.0.0.2");
    givenGlobalRegistryFactoryFunc(
        [this, &newVersion]()
        {
            return createUpdateRegistry(
                &newVersion,
                /*hasUpdate*/ true,
                /*expectFindUpdateFileWillBeCalled*/ false,
                /*expectToByteArrayWillBeCalled*/ false);
        });

    givenRemoteRegistryFactoryFunc(
        [this, &newVersion]()
        {
            return createUpdateRegistry(
                /*version*/ &newVersion,
                /*hasUpdate*/ true,
                /*expectFindUpdateFileWillBeCalled*/ false,
                /*expectToByteArrayWillBeCalled*/ false);
        });

    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::available);
}

// #TODO: #akulikov: Add tests regarding manual file data.

} // namespace test
} // namespace detail
} // namespace manager
} // namespace update
} // namespace nx
