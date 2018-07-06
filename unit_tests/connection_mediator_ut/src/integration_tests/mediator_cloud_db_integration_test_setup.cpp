#include "mediator_cloud_db_integration_test_setup.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace cloud {
namespace test {

MediatorCloudDbIntegrationTestSetup::MediatorCloudDbIntegrationTestSetup()
{
    nx::network::SocketGlobalsHolder::instance()->reinitialize();

    m_cdbLauncher = std::make_unique<cdb::CdbLauncher>();
    m_mediator = std::make_unique<hpm::MediatorFunctionalTest>(
        hpm::MediatorFunctionalTest::noFlags);
}

bool MediatorCloudDbIntegrationTestSetup::startCdb()
{
    return m_cdbLauncher->startAndWaitUntilStarted();
}

bool MediatorCloudDbIntegrationTestSetup::startMediator()
{
    QString cdbUrlArgument =
        lm("--cloud_db/url=http://%1/").arg(m_cdbLauncher->endpoint());
    m_mediator->addArg(cdbUrlArgument.toLatin1().constData());

    return m_mediator->startAndWaitUntilStarted();
}

cdb::CdbLauncher& MediatorCloudDbIntegrationTestSetup::cloudDb()
{
    return *m_cdbLauncher;
}

hpm::MediatorFunctionalTest& MediatorCloudDbIntegrationTestSetup::mediator()
{
    return *m_mediator;
}

} // namespace test
} // namespace cloud
} // namespace nx
