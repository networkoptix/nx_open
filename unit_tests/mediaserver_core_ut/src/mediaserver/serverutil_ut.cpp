#include <gtest/gtest.h>

#include <cstdlib>
#include <QString>
#include <QList>
#include <QFile>

#include <nx/utils/test_support/test_options.h>
#include <test_support/utils.h>

#include <media_server/serverutil.h>

TEST(serverutil, makeNextUniqueName)
{
    nx::ut::utils::WorkDirResource workDir(nx::utils::TestOptions::temporaryDirectoryPath());
    ASSERT_TRUE((bool)workDir.getDirName());
    const QString prefix = *workDir.getDirName()+lit("/ecs");
    QList<QString> stock;

    // step 1. Generate files
    for(int times=0; times<3; ++times) //< Yes, we need many name conflicts
    {
        const int maxBuild = 10 + std::rand() % 20;
        for (int build = 0; build < maxBuild; ++build)
        {
            const int maxIndex = std::rand() % 15;
            for (int index=0; index < maxIndex; ++index)
            {
                QString fileName = makeNextUniqueName(prefix, build);
                stock.push_back(fileName);
                QFile(fileName).open(QIODevice::WriteOnly);
            }
        }
    }

    // step 2. Check files
    int absentFiles = 0;
    for (const auto& fileName: stock)
    {
        if (!QFile::exists(fileName))
            ++absentFiles;
    }
    ASSERT_EQ(absentFiles, 0);
}
