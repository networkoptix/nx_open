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
            QString("/%1_ut.data").arg(moduleName);
    }
    m_tmpDir.removeRecursively();
    bool created = m_tmpDir.mkpath(m_tmpDir.absolutePath());
    NX_ASSERT(created);
}

TestWithTemporaryDirectory::~TestWithTemporaryDirectory()
{
    m_tmpDir.removeRecursively();
}

QString TestWithTemporaryDirectory::testDataDir() const
{
    return QDir::cleanPath(m_tmpDir.absolutePath());
}

} // namespace test
} // namespace utils
} // namespace nx
