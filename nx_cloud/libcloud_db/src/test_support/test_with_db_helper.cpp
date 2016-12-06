#include "test_with_db_helper.h"

#include <QtCore/QDir>

namespace nx {
namespace cdb {

QString TestWithDbHelper::sTemporaryDirectoryPath;
nx::db::ConnectionOptions TestWithDbHelper::sDbConnectionOptions;

TestWithDbHelper::TestWithDbHelper(QString tmpDir):
    m_tmpDir(tmpDir)
{
    if (m_tmpDir.isEmpty())
        m_tmpDir =
        (sTemporaryDirectoryPath.isEmpty() ? QDir::homePath() : sTemporaryDirectoryPath) +
        "/cdb_ut.data";
    QDir(m_tmpDir).removeRecursively();

    QDir().mkpath(testDataDir());

    m_dbConnectionOptions = sDbConnectionOptions;
    m_dbConnectionOptions.maxConnectionCount = 3; // TODO: #ak Make tunable

    if (m_dbConnectionOptions.dbName.isEmpty())
    {
        m_dbConnectionOptions.dbName =
            lit("%1/%2").arg(testDataDir()).arg(lit("cdb_ut.sqlite")).toLatin1().constData();
    }
}

TestWithDbHelper::~TestWithDbHelper()
{
    QDir(m_tmpDir).removeRecursively();
}

QString TestWithDbHelper::testDataDir() const
{
    return m_tmpDir;
}

const nx::db::ConnectionOptions& TestWithDbHelper::dbConnectionOptions() const
{
    return m_dbConnectionOptions;
}

nx::db::ConnectionOptions& TestWithDbHelper::dbConnectionOptions()
{
    return m_dbConnectionOptions;
}

void TestWithDbHelper::setTemporaryDirectoryPath(const QString& path)
{
    sTemporaryDirectoryPath = path;
}

QString TestWithDbHelper::temporaryDirectoryPath()
{
    return sTemporaryDirectoryPath;
}

void TestWithDbHelper::setDbConnectionOptions(
    const nx::db::ConnectionOptions& connectionOptions)
{
    sDbConnectionOptions = connectionOptions;
}

} // namespace cdb
} // namespace nx
