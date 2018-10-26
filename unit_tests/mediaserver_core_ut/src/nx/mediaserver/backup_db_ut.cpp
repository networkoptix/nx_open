#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/vms/utils/vms_utils.h>
#include <api/model/merge_system_data.h>
#include <api/model/configure_system_data.h>
#include <api/model/getnonce_reply.h>
#include <nx/vms/api/data/module_information.h>
#include <rest/server/json_rest_result.h>

#include <api/test_api_requests.h>
#include <test_support/utils.h>

namespace nx::test {

class BackupDb: public ::testing::Test
{
protected:
    enum class FileType
    {
        backup,
        nonBackup
    };

    using LauncherPtr = std::unique_ptr<MediaServerLauncher>;

    virtual void SetUp() override
    {
        m_server = std::make_unique<MediaServerLauncher>(/* tmpDir */ "");
        m_server->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
        m_baseDir = closeDirPath(m_server->dataDir());
    }

    void whenServerLaunched()
    {
        ASSERT_TRUE(m_server->start());
    }

    void whenSomeFilesCreated(int count, FileType fileType)
    {
        for (int i = 0; i < count; ++i)
        {
            const QString fileName = fileType == FileType::backup
                ? QString::fromLatin1("ec_%1_%2_%3.backup")
                    .arg(i * 1000)
                    .arg(i)
                    .arg(i + 1000LL * 3600LL * 24LL * 365LL * 2010LL)
                : notQuiteBackupFileName();

            const auto filePath = QDir(m_baseDir).absoluteFilePath(fileName);
            QList<QString>* containerToSaveIn = fileType == FileType::backup
                ? &m_backupFilesCreated
                : &m_nonBackupFilesCreated;

            containerToSaveIn->push_back(filePath);

            auto file = QFile(filePath);
            ASSERT_TRUE(file.open(QIODevice::WriteOnly));
            file.write("hello");
            file.close();
        }
    }

    void whenAllBackupFilesDataCalled()
    {
        m_backupFilesDataFound = nx::vms::utils::allBackupFilesDataSorted(m_baseDir);
    }

    void thenAllBackupFilesShouldBeFound()
    {
        ASSERT_EQ(m_backupFilesDataFound.size(), m_backupFilesCreated.size());

        for (const auto& backupFileData: m_backupFilesDataFound)
        {
            auto filePathIt = std::find_if(m_backupFilesCreated.begin(), m_backupFilesCreated.end(),
                [&backupFileData](const QString& filePath)
                {
                    return backupFileData.fullPath == filePath;
                });

            ASSERT_NE(filePathIt, m_backupFilesCreated.cend());
            m_backupFilesCreated.erase(filePathIt);
        }
    }

    void thenNoBackupFilesShouldBeCreated()
    {
        whenAllBackupFilesDataCalled();
        ASSERT_TRUE(m_backupFilesDataFound.isEmpty());
    }

    void givenDiskFreeSpace(qint64 freeSpace)
    {
        m_freeSpace = freeSpace;
    }

    void whenDeleteOldFilesFunctionCalled()
    {
        vms::utils::deleteOldBackupFilesIfNeeded(m_baseDir, m_freeSpace);
    }

    void thenOldestFilesShouldBeDeleted(int filesLeft)
    {
        const auto foundFiles = nx::vms::utils::allBackupFilesDataSorted(m_baseDir);
        ASSERT_EQ(filesLeft, foundFiles.size());
        ASSERT_EQ(foundFiles[0].timestamp, m_backupFilesDataFound[0].timestamp);
    }

    void whenServerStopped()
    {
        m_server->stop();
    }

    void thenBackupFilesShouldBeCreated(int backupFilesCount)
    {
        m_backupFilesDataFound = nx::vms::utils::allBackupFilesDataSorted(
            m_server->serverModule()->settings().backupDir());

        ASSERT_EQ(backupFilesCount, m_backupFilesDataFound.size());
    }

    void thenNoNewBackupFilesShouldBeCreated()
    {
    }

private:
    LauncherPtr m_server;
    QList<nx::vms::utils::DbBackupFileData> m_backupFilesDataFound;
    QList<QString> m_backupFilesCreated;
    QList<QString> m_nonBackupFilesCreated;
    QString m_baseDir;
    qint64 m_freeSpace = -1;

    static QString notQuiteBackupFileName()
    {
        const QList<QString> fileNames = {
            "ec_ab_1_100.backup",
            "ec_1_ab_100.backup",
            "ec_1_ab_cd.backup",
            "ec_1_ab_100.backup",
            "hello.txt",
            "looks_like_.backup"
        };

        return nx::utils::random::choice(fileNames);
    }
};

TEST_F(BackupDb, allBackupFilesData_correctnessCheck)
{
    whenSomeFilesCreated(/*count*/ 10, FileType::backup);
    whenSomeFilesCreated(/*count*/ 15, FileType::nonBackup);
    whenAllBackupFilesDataCalled();
    thenAllBackupFilesShouldBeFound();
}

TEST_F(BackupDb, rotation_freeSpaceMoreThan10Gb)
{
    givenDiskFreeSpace(11 * 1024 * 1024LL * 1024LL);
    whenSomeFilesCreated(/*count*/ 10, FileType::backup);
    whenAllBackupFilesDataCalled();
    whenDeleteOldFilesFunctionCalled();
    thenOldestFilesShouldBeDeleted(/*filesLeft*/ 6);
}

TEST_F(BackupDb, rotation_freeSpaceLessThan10Gb)
{
    givenDiskFreeSpace(9 * 1024 * 1024LL * 1024LL);
    whenSomeFilesCreated(/*count*/ 10, FileType::backup);
    whenAllBackupFilesDataCalled();
    whenDeleteOldFilesFunctionCalled();
    thenOldestFilesShouldBeDeleted(/*filesLeft*/ 1);
}

TEST_F(BackupDb, NoBackupCreatedOnFirstTimeLaunch)
{
    whenServerLaunched();
    thenNoBackupFilesShouldBeCreated();
}

TEST_F(BackupDb, CreatedWhenNoBackupsForCurrentVersion)
{
    whenServerLaunched();
    whenServerStopped();
    whenServerLaunched();
    thenBackupFilesShouldBeCreated(/*backupFilesCount*/ 1);
}

TEST_F(BackupDb, NotCreatedWhenThereAreBackupsForCurrentVersion)
{
    whenServerLaunched();
    whenServerStopped();
    whenServerLaunched();
    thenBackupFilesShouldBeCreated(/*backupFilesCount*/ 1);

    whenServerStopped();
    whenServerLaunched();
    thenNoNewBackupFilesShouldBeCreated();
}


} // namespace nx::mediaserver::test