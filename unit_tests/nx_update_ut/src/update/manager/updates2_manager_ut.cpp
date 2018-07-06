#include <nx/update/manager/detail/update_request_data_factory.h>
#include <nx/update/installer/detail/abstract_updates2_installer.h>
#include <nx/update/info/abstract_update_registry.h>
#include <nx/update/info/manual_file_data.h>
#include <nx/update/info/update_registry_factory.h>
#include <nx/update/manager/detail/updates2_manager_base.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/api//updates2/updates2_status_data.h>
#include <gtest/gtest.h>
#include <thread>

namespace nx {
namespace update {
namespace manager {
namespace detail {
namespace test {

using namespace ::testing;
using namespace info;
using namespace vms::common::p2p;

const static QString kFileName = "test.file.name";
const static QString kUpdatesUrl = "test.url";
const static QString kCloudHost = "test.cloud.host";
const static QString kCustomization = "test.customization";
const static nx::utils::SoftwareVersion kCurrentNxVersion("1.0.0.1");
const static nx::utils::SoftwareVersion kUpdateNxVersion("5.0.0.1");
const static QString kPlatform = "test.platform";
const static QString kArch = "test.arch";
const static QString kModification = "test.modification";

const static QString kFileUrl = "test.file.url";
const static int kFileSize = 42;
const static QByteArray kFileMd5 = "test.file.md5";

const static QString kManualFileName = "nxwitness-server_update-4.5.0.16975-linux64.zip";
const static QnUuid kCurrentPeerId = QnUuid::createUuid();

struct UpdateRegistryCfg
{
    bool hasUpdate = false;
};

class TestUpdateRegistry: public update::info::AbstractUpdateRegistry
{
public:

    void setCfg(const UpdateRegistryCfg& cfg) { m_cfg = cfg; }

    virtual info::ResultCode findUpdateFile(
        const info::UpdateRequestData& /*updateRequestData*/,
        info::FileData* outFileData) const override
    {
        if (!m_manualData.isEmpty())
        {
            outFileData->file = m_manualData[0].file;
            return info::ResultCode::ok;
        }

        if (m_cfg.hasUpdate)
        {
            outFileData->file = kFileName;
            outFileData->md5 = kFileMd5;
            outFileData->size = kFileSize;
            outFileData->url = kFileUrl;

            return info::ResultCode::ok;
        }

        return info::ResultCode::noData;
    }

    virtual info::ResultCode latestUpdate(
        const update::info::UpdateRequestData& /*updateRequestData*/,
        QList<api::TargetVersionWithEula>* outSoftwareVersions,
        QString* /*outReleaseNotesUrl*/) const override
    {
        if (m_cfg.hasUpdate || !m_manualData.isEmpty())
        {
            outSoftwareVersions->append(api::TargetVersionWithEula(kUpdateNxVersion));
            return info::ResultCode::ok;
        }

        return info::ResultCode::noData;
    }

    virtual void addFileData(const info::ManualFileData& manualFileData) override
    {
        m_manualData.append(manualFileData);
    }

    virtual void removeFileData(const QString& /*fileName*/) override
    {
        m_manualData.clear();
    }

    virtual QList<QString> alternativeServers() const override
    {
        return QStringList();
    }

    virtual int updateVersion() const
    {
        return 42;
    }

    virtual QByteArray toByteArray() const override
    {
        return "hello";
    }

    virtual bool fromByteArray(const QByteArray& /*rawData*/) override
    {
        return true;
    }

    virtual bool equals(AbstractUpdateRegistry* other) const override
    {
        if (!other)
            return false;

        QList<api::TargetVersionWithEula> targetVersions;
        return m_cfg.hasUpdate ==
            (other->latestUpdate(info::UpdateRequestData(), &targetVersions, nullptr) ==
                info::ResultCode::ok);
    }

    virtual void merge(AbstractUpdateRegistry* other) override
    {
        if (other)
        {
            QList<api::TargetVersionWithEula> targetVersions;
            m_cfg.hasUpdate |= (other->latestUpdate(info::UpdateRequestData(), &targetVersions, nullptr) ==
                info::ResultCode::ok);
        }
        m_manualData.append(((TestUpdateRegistry*) other)->m_manualData);
    }


    virtual QList<QnUuid> additionalPeers(const QString& /*fileName*/) const override
    {
        if (!m_manualData.isEmpty())
            return m_manualData[0].peers;

        return QList<QnUuid>();
    }

private:
    UpdateRegistryCfg m_cfg;
    QList<ManualFileData> m_manualData;
};

class TestDownloader: public downloader::AbstractDownloader
{
public:
    TestDownloader(): downloader::AbstractDownloader(nullptr) {}

    void setValidFileInformation()
    {
        m_fi.name = kFileName;
        m_fi.size = kFileSize;
        m_fi.md5 = kFileMd5;
        m_fi.status = downloader::FileInformation::Status::downloaded;
    }

    void setCorruptedFileInformation()
    {
        m_fi.name = kFileName;
        m_fi.size = kFileSize;
        m_fi.md5 = kFileMd5;
        m_fi.status = downloader::FileInformation::Status::corrupted;
    }

    void setAlreadyDownloadedFileInformation()
    {
        m_fi.name = kFileName;
        m_fi.size = kFileSize;
        m_fi.md5 = kFileMd5;
        m_fi.status = downloader::FileInformation::Status::downloaded;
    }

    void setExternallyAddedFileInformation(downloader::FileInformation::Status status)
    {
        m_fi.name = kManualFileName;
        m_fi.size = kFileSize;
        m_fi.md5 = kFileMd5;
        m_fi.status = status;
    }

    void setAddFileCode(downloader::ResultCode addFileCode)
    {
        m_addFileCode = addFileCode;
    }

    QList<QnUuid> additionalPeers() const
    {
        return m_additionalPeers;
    }

    virtual QStringList files() const override
    {
        return QStringList();
    }

    virtual QString filePath(const QString& /*fileName*/) const override
    {
        return "file.path";
    }

    virtual downloader::FileInformation fileInformation(const QString& /*fileName*/) const override
    {
        return m_fi;
    }

    virtual downloader::ResultCode addFile(
        const downloader::FileInformation& fileInformation) override
    {
        m_additionalPeers.append(fileInformation.additionalPeers);
        return m_addFileCode;
    }

    virtual downloader::ResultCode updateFileInformation(
        const QString& /*fileName*/,
        int /*size*/,
        const QByteArray& /*md5*/) override
    {
        return downloader::ResultCode::ok;
    }

    virtual downloader::ResultCode readFileChunk(
        const QString& /*fileName*/,
        int /*chunkIndex*/,
        QByteArray& /*buffer*/) override
    {
        return downloader::ResultCode::ok;
    }

    virtual downloader::ResultCode writeFileChunk(
        const QString& /*fileName*/,
        int /*chunkIndex*/,
        const QByteArray& /*buffer*/) override
    {
        return downloader::ResultCode::ok;
    }

    virtual downloader::ResultCode deleteFile(
        const QString& /*fileName*/,
        bool /*deleteData*/) override
    {
        return downloader::ResultCode::ok;
    }

    virtual QVector<QByteArray> getChunkChecksums(const QString& /*fileName*/) override
    {
        return QVector<QByteArray>();
    }

private:
    downloader::FileInformation m_fi;
    downloader::ResultCode m_addFileCode = downloader::ResultCode::ok;
    QList<QnUuid> m_additionalPeers;
};

enum class PrepareExpectedOutcome
{
    success,
    fail_noFreeSpace
};

class TestInstaller: public installer::detail::AbstractUpdates2Installer, public QnLongRunnable
{
public:
    virtual void prepareAsync(
        const QString& /*path*/,
        installer::detail::PrepareUpdateCompletionHandler handler) override
    {
        put(handler);
    }

    virtual bool install() override
    {
        return true;
    }

    virtual void stopSync() override
    {

    }

    void setExpectedOutcome(PrepareExpectedOutcome expectedOutcome)
    {
        m_expectedOutcome = expectedOutcome;
    }

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

class AbstractExternalsSupplier
{
public:
    virtual ~AbstractExternalsSupplier() {}
    virtual update::info::AbstractUpdateRegistryPtr getGlobalRegistry() = 0;
    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() = 0;
    virtual void updateGlobalRegistry(const UpdateRegistryCfg& cfg) = 0;
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() = 0;
    virtual installer::detail::AbstractUpdates2Installer* installer() = 0;
};

class TestUpdatesManager: public Updates2ManagerBase
{
public:
    TestUpdatesManager(AbstractExternalsSupplier* supplier):
        m_supplier(supplier)
    {}

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

    void emitFileInformationChanged(const downloader::FileInformation& fileInformation)
    {
        onFileInformationChanged(fileInformation);
    }

    void emitFileDeletedSignal()
    {
        onFileDeleted(kManualFileName);
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

    virtual qint64 refreshTimeout() const override { return 1000; }

    virtual QString filePath() const { return QLatin1Literal("some/path"); }

    virtual void loadStatusFromFile() override {}

    void connectToSignals() override {}

    virtual update::info::AbstractUpdateRegistryPtr getGlobalRegistry() override
    {
        return m_supplier->getGlobalRegistry();
    }

    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() override
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_getRemoteRegistryFinished)
            m_getRemoteRegistryCondition.wait(lock.mutex());

        return m_supplier->getRemoteRegistry();
    }

    virtual QnUuid moduleGuid() const override
    {
        static QnUuid guid = QnUuid::createUuid();
        return guid;
    }

    void updateGlobalRegistry(const QByteArray& /*serializedRegistry*/) override
    {
        UpdateRegistryCfg cfg;

        cfg.hasUpdate = true;
        m_supplier->updateGlobalRegistry(cfg);
    }

    virtual void writeStatusToFile(const detail::Updates2StatusDataEx& statusData) override
    {
        m_fileWrittenData.clone(statusData);
    }

    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() override
    {
        return m_supplier->downloader();
    }

    virtual installer::detail::AbstractUpdates2Installer* installer() override
    {
        return m_supplier->installer();
    }

    virtual void remoteUpdateCompleted() override
    {
        QnMutexLocker lock(&m_mutex);
        m_remoteUpdateFinished = true;
        m_remoteUpdateCondition.wakeOne();
    }

    virtual QnUuid peerId() const override
    {
        return kCurrentPeerId;
    }

    virtual bool isClient() const override
    {
        return false;
    }

private:
    AbstractExternalsSupplier* m_supplier;
    QnMutex m_mutex;
    QnWaitCondition m_remoteUpdateCondition;
    bool m_remoteUpdateFinished = false;
    detail::Updates2StatusDataEx m_fileWrittenData;
    QnWaitCondition m_getRemoteRegistryCondition;
    bool m_getRemoteRegistryFinished = true;
};

class Updates2Manager: public ::testing::Test, public AbstractExternalsSupplier
{
public:
    const nx::utils::SoftwareVersion kNewVersion = nx::utils::SoftwareVersion("1.0.0.2");

protected:
    virtual void SetUp() override
    {
        detail::UpdateRequestDataFactory::setFactoryFunc(
            [this]()
            {
                return update::info::UpdateRequestData(
                    kCloudHost,
                    kCustomization,
                    kCurrentNxVersion,
                    update::info::OsVersion(kPlatform, kArch, kModification),
                    nullptr,
                    false);
            });

        info::UpdateRegistryFactory::setEmptyFactoryFunction(
            [](const QnUuid& /*selfPeerId*/)
            {
                return std::make_unique<TestUpdateRegistry>();
            });

        m_updatesManager.reset(new TestUpdatesManager(this));
        m_installer.start();
    }

    virtual void TearDown() override
    {
        m_updatesManager->stopAsyncTasks();
        info::UpdateRegistryFactory::setEmptyFactoryFunction(nullptr);
    }

    virtual update::info::AbstractUpdateRegistryPtr getGlobalRegistry() override
    {
        return std::unique_ptr<TestUpdateRegistry>(new TestUpdateRegistry(m_globalRegistry));
    }

    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() override
    {
        return std::unique_ptr<TestUpdateRegistry>(new TestUpdateRegistry(m_remoteRegistry));
    }

    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() override
    {
        return &m_downloader;
    }

    virtual installer::detail::AbstractUpdates2Installer* installer() override
    {
        return &m_installer;
    }

    virtual void updateGlobalRegistry(const UpdateRegistryCfg& cfg)
    {
        m_globalRegistry.setCfg(cfg);
    }

    void givenUpdateRegistries(bool remoteHasUpdate, bool globalHasUpdate)
    {
        UpdateRegistryCfg cfg;

        cfg.hasUpdate = remoteHasUpdate;
        m_remoteRegistry.setCfg(cfg);

        cfg.hasUpdate = globalHasUpdate;
        m_globalRegistry.setCfg(cfg);
    }

    void whenServerHasBeenStarted()
    {
        m_updatesManager->atServerStart();
    }

    void thenStateShouldBe(
        api::Updates2StatusData::StatusCode status,
        const QString& message = QString())
    {
        ASSERT_EQ(status, m_updatesManager->status().state);
        if (!message.isEmpty())
        {
            ASSERT_TRUE(m_updatesManager->status().message.toLower().contains(message.toLower()));
        }
    }

    void thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode status)
    {
        while (m_updatesManager->status().state != status)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void whenRemoteUpdateDone()
    {
        m_updatesManager->waitForRemoteUpdate();
    }

    void whenRemoteUpdateTakesLongTimeToComplete()
    {
        m_updatesManager->setNeedToWaitForgetRemoteRegistry(true);
    }

    void whenRemoteUpdateFinishedAtLast()
    {
        m_updatesManager->setNeedToWaitForgetRemoteRegistry(false);
    }

    void whenDownloadRequestIssued(api::Updates2StatusData::StatusCode expectedResult)
    {
        while (expectedResult != m_updatesManager->download(kUpdateNxVersion).state)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void whenDownloadRequestIssuedWithFinalResult(api::Updates2StatusData::StatusCode expectedResult)
    {
        while (m_updatesManager->download(kUpdateNxVersion).state != expectedResult)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void whenDownloadFinishedSuccessfully()
    {
        m_downloader.setValidFileInformation();
        m_updatesManager->emitDownloadFinishedSignal(kFileName);
    }

    void whenExternalDownloadFinishedSuccessfully()
    {
        m_downloader.setExternallyAddedFileInformation(downloader::FileInformation::Status::downloaded);
        m_updatesManager->emitDownloadFinishedSignal(kManualFileName);
    }

    void whenDownloadFinishedUNSuccessfully()
    {
        m_downloader.setCorruptedFileInformation();
        m_updatesManager->emitDownloadFailedSignal(kFileName);
    }

    void whenDownloaderFailsToAddFile()
    {
        m_downloader.setAddFileCode(downloader::ResultCode::noFreeSpace);
    }

    void whenChunkDownloadFailedSignalReceived()
    {
        m_updatesManager->emitCunkDownloadFailedSignal(kFileName);
    }

    void givenFileHasAlreadyBeenDownloaded()
    {
        m_downloader.setAlreadyDownloadedFileInformation();
        m_downloader.setAddFileCode(downloader::ResultCode::fileAlreadyExists);
    }

    void givenFileHasBeenAddedExternally(downloader::FileInformation::Status status)
    {
        m_downloader.setExternallyAddedFileInformation(status);
        m_downloader.setAddFileCode(downloader::ResultCode::fileAlreadyExists);
    }

    void givenInstallerFailsWithNoFreeSpace()
    {
        m_installer.setExpectedOutcome(PrepareExpectedOutcome::fail_noFreeSpace);
    }

    void whenDownloaderDeleteFileSignalReceived()
    {
        m_updatesManager->emitFileDeletedSignal();
    }

    void thenAdditionalPeersShouldHaveBeenPassedToDownloader()
    {
        ASSERT_TRUE(m_downloader.additionalPeers().contains(kCurrentPeerId));
    }

    void whenDownloaderSetsBadStatusForFile()
    {
        m_downloader.setExternallyAddedFileInformation(downloader::FileInformation::Status::corrupted);
    }

    void whenDownloaderEmittedFileInformationSignalsWithProgress()
    {
        downloader::FileInformation fileInformation(kFileName);
        fileInformation.status = downloader::FileInformation::Status::downloading;
        fileInformation.downloadedChunks = QBitArray(32);

        m_progressReported.clear();
        for (int i = 0; i < fileInformation.downloadedChunks.size(); ++i)
        {
            fileInformation.downloadedChunks.setBit(i);
            m_updatesManager->emitFileInformationChanged(fileInformation);

            if (m_updatesManager->status().progress > 0.0f
                && (m_progressReported.isEmpty() ||
                    m_progressReported.last() != m_updatesManager->status().progress))
            {
                m_progressReported.append(m_updatesManager->status().progress);
            }
        }
    }

    void thenProgressShouldBeReflectedInManagerState()
    {
        ASSERT_GT(m_progressReported.size(), 10);
    }

    void whenDownloadCancelled()
    {
        m_updatesManager->cancel();
    }

private:
    std::unique_ptr<TestUpdatesManager> m_updatesManager;
    TestInstaller m_installer;
    TestDownloader m_downloader;
    TestUpdateRegistry m_remoteRegistry;
    TestUpdateRegistry m_globalRegistry;
    QList<double> m_progressReported;
};

TEST_F(Updates2Manager, NotAvailableStatusCheck)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ false, /*globalHasUpdate*/ false);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::notAvailable);
}

TEST_F(Updates2Manager, FoundGlobalUpdateRegistry_newVersion)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ false, /*globalHasUpdate*/ true);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, FoundRemoteUpdateRegistry_newVersion)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, StatusWhileCheckingForUpdate)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    whenRemoteUpdateTakesLongTimeToComplete();
    whenServerHasBeenStarted();
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::checking);
    whenRemoteUpdateFinishedAtLast();
    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, Download_successful)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();

    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::downloading);
    whenDownloadFinishedSuccessfully();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::preparing);
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::readyToInstall);
}

TEST_F(Updates2Manager, Download_additionalPeersFromManualData)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ false, /*globalHasUpdate*/ false);
    givenFileHasBeenAddedExternally(downloader::FileInformation::Status::downloaded);
    whenServerHasBeenStarted();
    whenExternalDownloadFinishedSuccessfully();

    whenDownloadRequestIssuedWithFinalResult(api::Updates2StatusData::StatusCode::preparing);
    thenAdditionalPeersShouldHaveBeenPassedToDownloader();
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::readyToInstall);
}

TEST_F(Updates2Manager, Download_externalFileAdded_wrongState)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ false, /*globalHasUpdate*/ false);
    givenFileHasBeenAddedExternally(downloader::FileInformation::Status::downloading);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();

    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::notAvailable);
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::notAvailable);
}

TEST_F(Updates2Manager, Download_externalFileDeleted)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ false, /*globalHasUpdate*/ false);
    givenFileHasBeenAddedExternally(downloader::FileInformation::Status::downloaded);
    whenServerHasBeenStarted();
    whenExternalDownloadFinishedSuccessfully();
    whenRemoteUpdateDone();

    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::available);

    whenDownloaderDeleteFileSignalReceived();
    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::notAvailable);
    whenRemoteUpdateDone();
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::notAvailable);
}

TEST_F(Updates2Manager, Download_addFile_fail)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();

    whenDownloaderFailsToAddFile();
    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::error);
    thenStateShouldBe(api::Updates2StatusData::StatusCode::error);
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, Download_notAvailableStateBecauseNoUpdate)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ false, /*globalHasUpdate*/ false);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();

    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::notAvailable);
    thenStateShouldBe(api::Updates2StatusData::StatusCode::notAvailable);
}

TEST_F(Updates2Manager, Download_notAvailableState_BecauseCheckHasNotFinished)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    whenRemoteUpdateTakesLongTimeToComplete();
    whenServerHasBeenStarted();
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::checking);
    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::checking);
    whenRemoteUpdateFinishedAtLast();
    whenRemoteUpdateDone();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, Download_alreadyDownloadingState)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();

    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::downloading);
    thenStateShouldBe(api::Updates2StatusData::StatusCode::downloading);
    whenDownloadFinishedSuccessfully();
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::readyToInstall);
}

TEST_F(Updates2Manager, Download_failed)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();

    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::downloading);
    whenDownloadFinishedUNSuccessfully();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::error);
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, Download_chunkFailedAndRecovered)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();

    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::downloading);
    whenChunkDownloadFailedSignalReceived();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::downloading, "problems");

    whenDownloadFinishedSuccessfully();
    thenStateShouldBe(api::Updates2StatusData::StatusCode::preparing);
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::readyToInstall);
}

TEST_F(Updates2Manager, Download_addAlreadyExistingFile)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    givenFileHasAlreadyBeenDownloaded();

    whenServerHasBeenStarted();
    whenRemoteUpdateDone();

    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::preparing);
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::readyToInstall);
}

TEST_F(Updates2Manager, Prepare_failedNoFreeSpace)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    givenInstallerFailsWithNoFreeSpace();
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();

    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::downloading);
    thenStateShouldBe(api::Updates2StatusData::StatusCode::downloading);
    whenDownloadFinishedSuccessfully();
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::error);
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::available);
}

TEST_F(Updates2Manager, Cancel_WhileDownloading)
{
    givenUpdateRegistries(/*remoteHasUpdate*/ true, /*globalHasUpdate*/ false);
    whenServerHasBeenStarted();
    whenRemoteUpdateDone();

    whenDownloadRequestIssued(api::Updates2StatusData::StatusCode::downloading);
    whenDownloaderEmittedFileInformationSignalsWithProgress();
    thenProgressShouldBeReflectedInManagerState();

    whenDownloadCancelled();
    thenStateShouldFinallyBecome(api::Updates2StatusData::StatusCode::notAvailable);
}

} // namespace test
} // namespace detail
} // namespace manager
} // namespace update
} // namespace nx
