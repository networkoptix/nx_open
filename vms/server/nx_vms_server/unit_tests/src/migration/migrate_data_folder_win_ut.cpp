#if defined (Q_OS_WIN)

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <array>
#include <misc/migrate_oldwin_dir.h>

class MigrateDataTestHandler : public nx::misc::MigrateDataHandler
{
public:
    MOCK_CONST_METHOD0(currentDataDir, QString());
    MOCK_CONST_METHOD0(windowsDir, QString());
    MOCK_CONST_METHOD1(dirExists, bool(const QString&));
    MOCK_METHOD1(makePath, bool(const QString&));
    MOCK_METHOD2(moveDir, bool(const QString&, const QString&));
    MOCK_METHOD1(rmDir, bool(const QString&));
};

using ::testing::Return;
using ::testing::_;

TEST(MigrateWinData, NoWinDir)
{
    MigrateDataTestHandler handler;
    EXPECT_CALL(handler, currentDataDir())
        .Times(1)
        .WillOnce(Return("c:\\windows\\data"));

    EXPECT_CALL(handler, windowsDir())
        .Times(1)
        .WillOnce(Return(QString()));

    ASSERT_EQ(nx::misc::migrateFilesFromWindowsOldDir(&handler),
              nx::misc::MigrateDataResult::WinDirNotFound);
}

TEST(MigrateWinData, CurrentDataDirNotInWindowsFolder)
{
    MigrateDataTestHandler handler;
    EXPECT_CALL(handler, currentDataDir())
        .Times(1)
        .WillOnce(Return("c:\\data"));

    EXPECT_CALL(handler, windowsDir())
        .Times(1)
        .WillOnce(Return("c:\\windows"));

    ASSERT_EQ(nx::misc::migrateFilesFromWindowsOldDir(&handler),
              nx::misc::MigrateDataResult::NoNeedToMigrate);
}

TEST(MigrateWinData, OldDirDoesntExist)
{
    MigrateDataTestHandler handler;
    EXPECT_CALL(handler, currentDataDir())
        .WillRepeatedly(Return("c:\\windows\\data"));

    EXPECT_CALL(handler, windowsDir())
        .WillRepeatedly(Return("c:\\windows"));

    EXPECT_CALL(handler, dirExists(_))
        .WillRepeatedly(Return(false));

    ASSERT_EQ(nx::misc::migrateFilesFromWindowsOldDir(&handler),
              nx::misc::MigrateDataResult::NoNeedToMigrate);
}

using ::testing::InSequence;


enum class MoveData
{
    Successfull,
    Failed
};

void whenEverythingAlmostOk(MigrateDataTestHandler& handler, MoveData moveDataFlag, const QString& oldDataDir)
{
    EXPECT_CALL(handler, currentDataDir())
        .WillRepeatedly(Return("c:\\windows\\data"));

    EXPECT_CALL(handler, windowsDir())
        .WillRepeatedly(Return("c:\\windows"));

    EXPECT_CALL(handler, dirExists(_))
        .WillRepeatedly(Return(false));

    EXPECT_CALL(handler, dirExists(oldDataDir))
        .WillOnce(Return(true));

    EXPECT_CALL(handler, makePath(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(handler, rmDir(_)).WillRepeatedly(Return(true));

    if (moveDataFlag == MoveData::Successfull)
        EXPECT_CALL(handler, moveDir(_,_)).WillRepeatedly(Return(true));
    else
        EXPECT_CALL(handler, moveDir(_,_)).WillRepeatedly(Return(false));
}

TEST(MigrateWinData, VariousOldDirs)
{
    MigrateDataTestHandler handler;

    std::array<QString, 2> oldDataDirPaths = {
        "c:\\windows.old\\data",
        "c:\\windows.000\\data"
    };

    for (const auto oldDataDir: oldDataDirPaths)
    {
        whenEverythingAlmostOk(handler, MoveData::Successfull, oldDataDir);

        ASSERT_EQ(nx::misc::migrateFilesFromWindowsOldDir(&handler),
                  nx::misc::MigrateDataResult::Ok);
    }
}

TEST(MigrateWinData, MoveDataFailed)
{
    MigrateDataTestHandler handler;
    whenEverythingAlmostOk(handler, MoveData::Failed, "c:\\windows.000\\data");

    ASSERT_EQ(nx::misc::migrateFilesFromWindowsOldDir(&handler),
              nx::misc::MigrateDataResult::MoveDataFailed);
}

#endif // Q_OS_WIN