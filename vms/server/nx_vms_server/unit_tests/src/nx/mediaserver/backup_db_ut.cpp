#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/vms/utils/vms_utils.h>
#include <api/model/merge_system_data.h>
#include <api/model/configure_system_data.h>
#include <api/model/getnonce_reply.h>
#include <nx/vms/api/data/module_information.h>
#include <rest/server/json_rest_result.h>
#include <nx/system_commands.h>

#include <api/test_api_requests.h>
#include <test_support/utils.h>

namespace nx::test {


class BackupDbUt: public ::testing::Test
{
protected:
    enum class FileType
    {
        backup,
        nonBackup
    };

    virtual void SetUp() override
    {
        m_testDir = *m_dirResource.getDirName();
    }

    void whenSomeFilesCreated(int count, FileType fileType)
    {
        for (int i = 0; i < count; ++i)
        {
            const QString fileName = fileType == FileType::backup
                ? QString::fromLatin1("ec_%1_%3.backup")
                    .arg(i * 1000)
                    .arg(i + 1000LL * 3600LL * 24LL * 365LL * 2010LL)
                : notQuiteBackupFileName();

            const auto filePath = QDir(m_testDir).absoluteFilePath(fileName);
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
        m_backupFilesDataFound = nx::vms::utils::allBackupFilesDataSorted(m_testDir);
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

        ASSERT_TRUE(std::is_sorted(m_backupFilesDataFound.cbegin(), m_backupFilesDataFound.cend(),
            [](const auto& lhs, const auto& rhs) { return rhs.timestamp < lhs.timestamp; }));
    }

    void givenDiskFreeSpace(qint64 freeSpace)
    {
        m_freeSpace = freeSpace;
    }

    void whenDeleteOldFilesFunctionCalled()
    {
        vms::utils::deleteOldBackupFilesIfNeeded(m_testDir, m_freeSpace);
    }

    void thenOldestFilesShouldBeDeleted(int filesLeft)
    {
        const auto foundFiles = nx::vms::utils::allBackupFilesDataSorted(m_testDir);
        ASSERT_EQ(filesLeft, foundFiles.size());
        ASSERT_EQ(foundFiles[0].timestamp, m_backupFilesDataFound[0].timestamp);
    }

private:
    QList<nx::vms::utils::DbBackupFileData> m_backupFilesDataFound;
    QList<QString> m_backupFilesCreated;
    QList<QString> m_nonBackupFilesCreated;
    nx::ut::utils::WorkDirResource m_dirResource;
    QString m_testDir;
    qint64 m_freeSpace = -1;

    static QString notQuiteBackupFileName()
    {
        const QList<QString> fileNames = {
            "ec_ab_100.backup",
            "ec_ab_cd.backup",
            "hello.txt",
            "looks_like_.backup"
        };

        return nx::utils::random::choice(fileNames);
    }
 };

class BackupDbIt: public ::testing::Test
{
protected:
    using LauncherPtr = std::unique_ptr<MediaServerLauncher>;

    virtual void SetUp() override
    {
        m_server = std::make_unique<MediaServerLauncher>(/* tmpDir */ "");
        m_server->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
        m_server->addSetting("dataDir", *m_dirResource.getDirName());
    }

    void whenServerLaunched()
    {
        ASSERT_TRUE(m_server->start());
        m_backupDir = m_server->serverModule()->settings().backupDir();
        m_dataDir = m_server->serverModule()->settings().dataDir();
    }

    void whenServerStopped()
    {
        m_server->stop();
    }

    void thenBackupFilesShouldBeCreated(int backupFilesCount)
    {
        m_backupFilesDataFound = nx::vms::utils::allBackupFilesDataSorted(m_backupDir);
        ASSERT_EQ(backupFilesCount, m_backupFilesDataFound.size());
    }

    void thenNoNewBackupFilesShouldBeCreated()
    {
        const auto backupFilesData = nx::vms::utils::allBackupFilesDataSorted(m_backupDir);
        ASSERT_EQ(m_backupFilesDataFound[0].timestamp, backupFilesData[0].timestamp);
    }

    void thenNoBackupFilesShouldBeCreated()
    {
        const auto backupFilesData = nx::vms::utils::allBackupFilesDataSorted(m_backupDir);
        ASSERT_TRUE(backupFilesData.isEmpty());
    }

    void givenBackupRotationPeriod(std::chrono::milliseconds period)
    {
        m_server->addSetting("dbBackupPeriodMS",
            QString::fromStdString(std::to_string(period.count())));
    }

    void thenSeveralRotationsShouldBeObserved()
    {
        waitForAllBackupFilesToBeCreated();

        QList<nx::vms::utils::DbBackupFileData> oldBackupFilesData;
        int kMaxRotationCount = 10;
        int rotations = 0;

        while (rotations < kMaxRotationCount)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            const auto backupFilesData = nx::vms::utils::allBackupFilesDataSorted(m_backupDir);

            if (oldBackupFilesData.isEmpty())
            {
                oldBackupFilesData = backupFilesData;
                continue;
            }
            else
            {
                ASSERT_TRUE(std::abs(backupFilesData.size() - oldBackupFilesData.size()) <= 1);
                if (backupFilesData[0].timestamp != oldBackupFilesData[0].timestamp)
                    ++rotations;
            }

            oldBackupFilesData = backupFilesData;
        }
    }

private:
    LauncherPtr m_server;
    QList<nx::vms::utils::DbBackupFileData> m_backupFilesDataFound;
    QList<QString> m_backupFilesCreated;
    QString m_backupDir;
    QString m_dataDir;
    nx::ut::utils::WorkDirResource m_dirResource;

    void waitForAllBackupFilesToBeCreated()
    {
        const auto diskFreeSpace = nx::SystemCommands().freeSpace(m_dataDir.toStdString());
        const int kMaxBackupFilesCount = diskFreeSpace > 10 * 1024LL * 1024LL * 1024LL ? 6 : 1;

        while (m_backupFilesDataFound.size() != kMaxBackupFilesCount)
        {
            m_backupFilesDataFound = nx::vms::utils::allBackupFilesDataSorted(m_backupDir);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

TEST_F(BackupDbUt, allBackupFilesData_correctnessCheck)
{
    whenSomeFilesCreated(/*count*/ 10, FileType::backup);
    whenSomeFilesCreated(/*count*/ 15, FileType::nonBackup);
    whenAllBackupFilesDataCalled();
    thenAllBackupFilesShouldBeFound();
}

TEST_F(BackupDbUt, rotation_freeSpaceMoreThan10Gb)
{
    givenDiskFreeSpace(11 * 1024 * 1024LL * 1024LL);
    whenSomeFilesCreated(/*count*/ 10, FileType::backup);
    whenAllBackupFilesDataCalled();
    whenDeleteOldFilesFunctionCalled();
    thenOldestFilesShouldBeDeleted(/*filesLeft*/ 6);
}

TEST_F(BackupDbUt, rotation_freeSpaceLessThan10Gb)
{
    givenDiskFreeSpace(9 * 1024 * 1024LL * 1024LL);
    whenSomeFilesCreated(/*count*/ 10, FileType::backup);
    whenAllBackupFilesDataCalled();
    whenDeleteOldFilesFunctionCalled();
    thenOldestFilesShouldBeDeleted(/*filesLeft*/ 1);
}

TEST_F(BackupDbIt, NoBackupCreatedOnFirstTimeLaunch)
{
    whenServerLaunched();
    thenNoBackupFilesShouldBeCreated();
}

TEST_F(BackupDbIt, CreatedWhenNoBackupsForCurrentVersion)
{
    whenServerLaunched();
    whenServerStopped();
    whenServerLaunched();
    thenBackupFilesShouldBeCreated(/*backupFilesCount*/ 1);
}

TEST_F(BackupDbIt, NotCreatedWhenThereAreBackupsForCurrentVersion)
{
    whenServerLaunched();
    whenServerStopped();
    whenServerLaunched();
    thenBackupFilesShouldBeCreated(/*backupFilesCount*/ 1);

    whenServerStopped();
    whenServerLaunched();
    thenNoNewBackupFilesShouldBeCreated();
}

TEST_F(BackupDbIt, FilesRotated)
{
    givenBackupRotationPeriod(std::chrono::milliseconds(1000));
    whenServerLaunched();
    thenSeveralRotationsShouldBeObserved();
}

} // namespace nx::mediaserver::test