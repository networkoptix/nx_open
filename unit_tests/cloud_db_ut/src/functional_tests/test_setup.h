/**********************************************************
* Sep 29, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_UT_TEST_SETUP_H
#define NX_CDB_UT_TEST_SETUP_H

#include <gtest/gtest.h>

#include <test_support/cdb_launcher.h>


namespace nx {
namespace cdb {

class CdbFunctionalTest
:
    public CdbLauncher,
    public ::testing::Test
{
public:
    //!Calls \a start
    CdbFunctionalTest();
    ~CdbFunctionalTest();

    /**
     * Convenience functions.
     * Most methods do assert on failure.
     */
    
    AccountWithPassword addActivatedAccount2();
    api::SystemData addRandomSystemToAccount(
        const AccountWithPassword& account);
    void shareSystem2(
        const AccountWithPassword& from,
        const api::SystemData& what,
        const AccountWithPassword& to,
        api::SystemAccessRole targetRole);
};

}   //cdb
}   //nx

#endif  //NX_CDB_UT_TEST_SETUP_H
