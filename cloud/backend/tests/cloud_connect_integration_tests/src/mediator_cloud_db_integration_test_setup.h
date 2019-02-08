#pragma once

#include <memory>

#include <nx/cloud/db/test_support/cdb_launcher.h>
#include <nx/cloud/mediator/test_support/mediator_functional_test.h>

namespace nx {
namespace cloud {
namespace test {

class MediatorCloudDbIntegrationTestSetup
{
public:
    MediatorCloudDbIntegrationTestSetup();

    bool startCdb();
    bool startMediator();

    nx::cloud::db::CdbLauncher& cloudDb();
    hpm::MediatorFunctionalTest& mediator();

private:
    std::unique_ptr<nx::cloud::db::CdbLauncher> m_cdbLauncher;
    std::unique_ptr<hpm::MediatorFunctionalTest> m_mediator;
};

} // namespace test
} // namespace cloud
} // namespace nx
