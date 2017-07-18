#pragma once

#include <memory>

#include <nx/cloud/cdb/test_support/cdb_launcher.h>
#include <test_support/mediator_functional_test.h>

namespace nx {
namespace cloud {
namespace test {

class MediatorCloudDbIntegrationTestSetup
{
public:
    MediatorCloudDbIntegrationTestSetup();

    bool startCdb();
    bool startMediator();

    cdb::CdbLauncher& cloudDb();
    hpm::MediatorFunctionalTest& mediator();

private:
    std::unique_ptr<cdb::CdbLauncher> m_cdbLauncher;
    std::unique_ptr<hpm::MediatorFunctionalTest> m_mediator;
};

} // namespace test
} // namespace cloud
} // namespace nx
