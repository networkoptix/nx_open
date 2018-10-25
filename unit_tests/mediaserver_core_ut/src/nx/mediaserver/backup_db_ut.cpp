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
    }

    void whenServerLaunched()
    {
        ASSERT_TRUE(m_server->start());
    }

    void whenSomeFilesCreated(int count, FileType fileType)
    {
        const auto baseDir = closeDirPath(ut::cfg::configInstance().tmpDir);
        for (int i = 0; i < count; ++i)
        {
            const QString fileName = fileType == FileType::backup
                ? QString::fromLatin1("ec_%1_%2_%3.backup")
                    .arg(i * 1000)
                    .arg(i)
                    .arg(i + 1000LL * 3600LL * 24LL * 365LL * 2010LL)
                : notQuiteBackupFileName();

            const auto filePath = QDir(baseDir).absoluteFilePath(fileName);
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
        m_backupFilesDataFound =
            nx::vms::utils::allBackupFilesData(closeDirPath(ut::cfg::configInstance().tmpDir));
    }

    void thenAllBackupFilesShouldBeFound()
    {
        ASSERT_EQ(m_backupFilesDataFound.size(), m_backupFilesCreated.size());

        for (const auto& backupFileData: m_backupFilesDataFound)
        {
            auto filePathIt = std::find_if(
                m_backupFilesCreated.begin(),
                m_backupFilesCreated.end(),
                [&backupFileData](const QString& filePath)
                {
                    return backupFileData.fullPath == filePath;
                });

            ASSERT_NE(filePathIt, m_backupFilesCreated.cend());
            m_backupFilesCreated.erase(filePathIt);
        }
    }

    void thanNoBackupFilesShouldBeCreated()
    {

    }

private:
    LauncherPtr m_server;
    QList<nx::vms::utils::DbBackupFileData> m_backupFilesDataFound;
    QList<QString> m_backupFilesCreated;
    QList<QString> m_nonBackupFilesCreated;

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

TEST_F(BackupDb, NoBackupCreatedOnFirstTimeLaunch)
{
    whenServerLaunched();
    thanNoBackupFilesShouldBeCreated();
}

} // namespace nx::mediaserver::test