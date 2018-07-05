#pragma once

#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/std/optional.h>

#include "../types.h"

namespace nx::sql::test {

class NX_SQL_API TestWithDbHelper:
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

    static std::optional<nx::sql::ConnectionOptions> sDbConnectionOptions;

    void cleanDatabase();
};

} // namespace nx::sql::test
