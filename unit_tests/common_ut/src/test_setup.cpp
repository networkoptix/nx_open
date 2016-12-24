
#include "test_setup.h"

static QString temporaryDirectoryPath("common_ut_tmp");

void TestSetup::setTemporaryDirectoryPath(const QString& path)
{
    temporaryDirectoryPath = path;
}

QString TestSetup::getTemporaryDirectoryPath()
{
    return temporaryDirectoryPath;
}
