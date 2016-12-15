#pragma once

#include <boost/optional.hpp>

#include <QtCore/QString>

#include <utils/db/types.h>

namespace nx {
namespace db {
namespace test {

class TestWithDbHelper
{
public:
    TestWithDbHelper(QString moduleName, QString tmpDir);
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
    static boost::optional<nx::db::ConnectionOptions> sDbConnectionOptions;
};

} // namespace test
} // namespace db
} // namespace nx
