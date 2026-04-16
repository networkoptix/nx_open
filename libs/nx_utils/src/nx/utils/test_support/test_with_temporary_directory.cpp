// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_with_temporary_directory.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>

#include "test_options.h"

#include <nx/utils/log/log.h>

namespace nx::utils::test {

namespace {

QString calculateTestDirectory(const QString& tmpDir)
{
    // Explicitly passed directory has the highest priority.
    if (!tmpDir.isEmpty())
        return tmpDir;

    // Always generating different directories so that multiple
    // TestWithTemporaryDirectory instances in a single test do not collide.
    static std::atomic<int> sequence{0};

    const QString moduleSuffix = QString("/%1_%2.tst")
        .arg(TestOptions::moduleName()).arg(++sequence);

    // Global temp dir is used if available. Module name is used as a subfolder.
    if (auto globalTmpDir = TestOptions::temporaryDirectoryPath(); !globalTmpDir.isEmpty())
        return globalTmpDir + moduleSuffix;

    // Fallback to home directory with module name subfolder.
    return QDir::homePath() + moduleSuffix;
}

} // namespace

TestWithTemporaryDirectory::TestWithTemporaryDirectory(
    const QString& tmpDir)
    :
    m_tmpDir(calculateTestDirectory(tmpDir))
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
        if (!removed)
            listDir();
        NX_ASSERT(removed);
    }
}

void TestWithTemporaryDirectory::listDir() const
{
    QDirIterator it(m_tmpDir.absolutePath(),
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    QString contents;
    while (it.hasNext())
        contents += "\n  " + it.next();
    NX_WARNING(this, "Failed to remove directory %1:%2", m_tmpDir.absolutePath(), contents);
}

QString TestWithTemporaryDirectory::testDataDir() const
{
    return QDir::cleanPath(m_tmpDir.absolutePath());
}

} // namespace nx::utils::test
