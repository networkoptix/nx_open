#include "test_with_temporary_directory.h"

#include <QtCore/QDir>

#include "test_options.h"

#include <nx/utils/log/log.h>

namespace nx::utils::test {

namespace {

QString calculateTestDirectory(const QString& moduleName, const QString& tmpDir)
{
    // Explitly passed directory has the highest priority.
    if (!tmpDir.isEmpty())
        return tmpDir;

    const QString moduleSuffix = QString("/%1_ut.data").arg(moduleName);

    // Global temp dir is used if available. Module name is used as a subfolder.
    if (auto globalTmpDir = TestOptions::temporaryDirectoryPath(); !globalTmpDir.isEmpty())
        return globalTmpDir + moduleSuffix;

    // Fallback to home directory with module name subfolder.
    return QDir::homePath() + moduleSuffix;
}

} // namespace

TestWithTemporaryDirectory::TestWithTemporaryDirectory(
    const QString& moduleName,
    const QString& tmpDir)
    :
    m_tmpDir(calculateTestDirectory(moduleName, tmpDir))
{
    // Auto-clean temporary folder contents before testing.
    m_tmpDir.removeRecursively();
    const bool created = m_tmpDir.mkpath(m_tmpDir.absolutePath());
    NX_ASSERT(created);
}

TestWithTemporaryDirectory::~TestWithTemporaryDirectory()
{
    if (!TestOptions::keepTemporaryDirectory())
    {
        const bool removed = m_tmpDir.removeRecursively();
        NX_ASSERT(removed);
    }
}

QString TestWithTemporaryDirectory::testDataDir() const
{
    return QDir::cleanPath(m_tmpDir.absolutePath());
}

} // namespace nx::utils::test
