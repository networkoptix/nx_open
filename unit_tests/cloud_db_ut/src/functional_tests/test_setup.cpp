/**********************************************************
* Sep 29, 2015
* akolesnikov
***********************************************************/

#include "test_setup.h"

#include <gtest/gtest.h>

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

}   //cdb
}   //nx

