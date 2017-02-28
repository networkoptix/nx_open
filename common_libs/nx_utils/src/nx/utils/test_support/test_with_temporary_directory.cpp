#include "test_with_temporary_directory.h"

#include <QtCore/QDir>

namespace nx {
namespace utils {
namespace test {

QString TestWithTemporaryDirectory::sTemporaryDirectoryPath;

TestWithTemporaryDirectory::TestWithTemporaryDirectory(QString moduleName, QString tmpDir):
    m_tmpDir(tmpDir)
{
    if (m_tmpDir.isEmpty())
    {
        m_tmpDir =
            (sTemporaryDirectoryPath.isEmpty() ? QDir::homePath() : sTemporaryDirectoryPath) +
            lit("/%1_ut.data").arg(moduleName);
    }
    QDir(m_tmpDir).removeRecursively();
    QDir().mkpath(m_tmpDir);
}

TestWithTemporaryDirectory::~TestWithTemporaryDirectory()
{
    QDir(m_tmpDir).removeRecursively();
}

QString TestWithTemporaryDirectory::testDataDir() const
{
    return m_tmpDir;
}

void TestWithTemporaryDirectory::setTemporaryDirectoryPath(const QString& path)
{
    sTemporaryDirectoryPath = path;
}

QString TestWithTemporaryDirectory::temporaryDirectoryPath()
{
    return sTemporaryDirectoryPath;
}

} // namespace test
} // namespace utils
} // namespace nx
