#pragma once

#include <gtest/gtest.h>

#include <nx/cloud/cdb/test_support/cdb_launcher.h>

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

private:
};

} // namespace cdb
} // namespace nx
