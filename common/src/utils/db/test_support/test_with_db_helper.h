#pragma once

#include <boost/optional.hpp>

#include <nx/utils/test_support/test_with_temporary_directory.h>

#include "../types.h"

namespace nx {
namespace db {
namespace test {

class TestWithDbHelper:
    public utils::test::TestWithTemporaryDirectory
{
public:
    TestWithDbHelper(QString moduleName, QString tmpDir);
    ~TestWithDbHelper();

    const nx::db::ConnectionOptions& dbConnectionOptions() const;
    nx::db::ConnectionOptions& dbConnectionOptions();

    static void setDbConnectionOptions(
        const nx::db::ConnectionOptions& connectionOptions);

private:
    nx::db::ConnectionOptions m_dbConnectionOptions;

    static boost::optional<nx::db::ConnectionOptions> sDbConnectionOptions;
};

} // namespace test
} // namespace db
} // namespace nx
