/**********************************************************
* Sep 29, 2015
* akolesnikov
***********************************************************/

#include "test_setup.h"

#include <gtest/gtest.h>

#include <nx/utils/test_support/utils.h>

#include "email_manager_mocked.h"

namespace nx {
namespace cdb {

CdbFunctionalTest::CdbFunctionalTest()
{
}

CdbFunctionalTest::~CdbFunctionalTest()
{
    stop();
}

} // namespace cdb
} // namespace nx
