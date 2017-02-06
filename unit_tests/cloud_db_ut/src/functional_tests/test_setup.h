#pragma once

#include <gtest/gtest.h>

#include <test_support/cdb_launcher.h>

namespace nx {
namespace cdb {

class CdbFunctionalTest:
    public CdbLauncher,
    public ::testing::Test
{
public:
    /** Calls CdbLauncher::start. */
    CdbFunctionalTest();
    ~CdbFunctionalTest();

    /**
     * Convenience functions.
     * Most methods do assert on failure.
     */
    
    AccountWithPassword addActivatedAccount2();
    api::SystemData addRandomSystemToAccount(
        const AccountWithPassword& account);
    api::SystemData addRandomSystemToAccount(
        const AccountWithPassword& account,
        const api::SystemData& systemPrototype);
    void shareSystemEx(
        const AccountWithPassword& from,
        const api::SystemData& what,
        const AccountWithPassword& to,
        api::SystemAccessRole targetRole);
    void shareSystemEx(
        const AccountWithPassword& from,
        const api::SystemData& what,
        const std::string& emailToShareWith,
        api::SystemAccessRole targetRole);

    void enableUser(
        const AccountWithPassword& who,
        const api::SystemData& what,
        const AccountWithPassword& whom);
    void disableUser(
        const AccountWithPassword& who,
        const api::SystemData& what,
        const AccountWithPassword& whom);

private:
    void setUserEnabledFlag(
        const AccountWithPassword& who,
        const api::SystemData& what,
        const AccountWithPassword& whom,
        bool isEnabled);
};

} // namespace cdb
} // namespace nx
