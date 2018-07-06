#pragma once

#include <boost/optional.hpp>

#include <nx/utils/test_support/test_with_temporary_directory.h>

#include "../types.h"

namespace nx {
namespace utils {
namespace db {
namespace test {

class NX_UTILS_API TestWithDbHelper:
    public utils::test::TestWithTemporaryDirectory
{
public:
    TestWithDbHelper(QString moduleName, QString tmpDir);
    ~TestWithDbHelper();

    const nx::utils::db::ConnectionOptions& dbConnectionOptions() const;
    nx::utils::db::ConnectionOptions& dbConnectionOptions();

    static void setDbConnectionOptions(
        const nx::utils::db::ConnectionOptions& connectionOptions);

private:
    nx::utils::db::ConnectionOptions m_dbConnectionOptions;

    static boost::optional<nx::utils::db::ConnectionOptions> sDbConnectionOptions;

    void cleanDatabase();
};

} // namespace test
} // namespace db
} // namespace utils
} // namespace nx
