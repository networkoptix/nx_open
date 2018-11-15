#include "test_setup.h"

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/test_support/utils.h>

#include "email_manager_mocked.h"

namespace nx::cloud::db {
namespace test {

CdbFunctionalTest::CdbFunctionalTest()
{
    NX_DEBUG(this, lm("============== Running test %1.%2 ==============")
        .arg(::testing::UnitTest::GetInstance()->current_test_info()->test_case_name())
        .arg(::testing::UnitTest::GetInstance()->current_test_info()->name()));
}

CdbFunctionalTest::~CdbFunctionalTest()
{
    stop();
}

} // namespace test
} // namespace nx::cloud::db
