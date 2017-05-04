#include "test_with_temporary_directory.h"

#include <QtCore/QDir>

#include "test_options.h"

namespace nx {
namespace utils {
namespace test {

TestWithTemporaryDirectory::TestWithTemporaryDirectory(QString moduleName, QString tmpDir):
    m_tmpDir(tmpDir)
{
    if (m_tmpDir.isEmpty())
    {
        m_tmpDir =
            (TestOptions::temporaryDirectoryPath().isEmpty()
                ? QDir::homePath() : TestOptions::temporaryDirectoryPath()) +
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

} // namespace test
} // namespace utils
} // namespace nx
