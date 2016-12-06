#pragma once

#include <QtCore/QString>

#include <utils/db/types.h>

namespace nx {
namespace cdb {

class TestWithDbHelper
{
public:
    TestWithDbHelper(QString tmpDir = QString());
    ~TestWithDbHelper();

    QString testDataDir() const;

    const nx::db::ConnectionOptions& dbConnectionOptions() const;
    nx::db::ConnectionOptions& dbConnectionOptions();

    static void setTemporaryDirectoryPath(const QString& path);
    static QString temporaryDirectoryPath();

    static void setDbConnectionOptions(
        const nx::db::ConnectionOptions& connectionOptions);

private:
    QString m_tmpDir;
    nx::db::ConnectionOptions m_dbConnectionOptions;

    static QString sTemporaryDirectoryPath;
    static nx::db::ConnectionOptions sDbConnectionOptions;
};

} // namespace cdb
} // namespace nx
