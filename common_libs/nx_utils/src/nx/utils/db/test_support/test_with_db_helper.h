#pragma once

#include <boost/optional.hpp>

#include <nx/utils/test_support/test_with_temporary_directory.h>

#include "../types.h"

namespace nx {
namespace sql {
namespace test {

class NX_UTILS_API TestWithDbHelper:
    public utils::test::TestWithTemporaryDirectory
{
public:
    TestWithDbHelper(QString moduleName, QString tmpDir);
    ~TestWithDbHelper();

    const nx::sql::ConnectionOptions& dbConnectionOptions() const;
    nx::sql::ConnectionOptions& dbConnectionOptions();

    static void setDbConnectionOptions(
        const nx::sql::ConnectionOptions& connectionOptions);

private:
    nx::sql::ConnectionOptions m_dbConnectionOptions;

    static boost::optional<nx::sql::ConnectionOptions> sDbConnectionOptions;

    void cleanDatabase();
};

} // namespace test
} // namespace sql
} // namespace nx
