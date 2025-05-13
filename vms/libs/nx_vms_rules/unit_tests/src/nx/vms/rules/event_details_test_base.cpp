// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_details_test_base.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/resource/resource_pool_test_helper.h>
#include <nx/vms/time/formatter.h>

#include "test_router.h"

namespace nx::vms::rules::test {

namespace {

static std::unique_ptr<common::SystemContext> sContext;
static std::unique_ptr<Engine> sEngine;

} // namespace

const QString EventDetailsTestBase::kSystemName = "System Name";
const QString EventDetailsTestBase::kSystemSignature = "System Signature";

const Uuid EventDetailsTestBase::kServerId = Uuid::createUuid();
const QString EventDetailsTestBase::kServerName = "Server1";

const QString EventDetailsTestBase::kServer2Name = "Server2";
const Uuid EventDetailsTestBase::kServer2Id = Uuid::createUuid();

const QString EventDetailsTestBase::kEngine1Name = "Engine1";
const Uuid EventDetailsTestBase::kEngine1Id = nx::Uuid::createUuid();

const QString EventDetailsTestBase::kCamera1Name = "Entrance";
const Uuid EventDetailsTestBase::kCamera1Id = Uuid::createUuid();
const QString EventDetailsTestBase::kCamera1PhysicalId = "2a:cf:c7:04:c7:c9";

const QString EventDetailsTestBase::kCamera2Name = "Camera2";
const Uuid EventDetailsTestBase::kCamera2Id = Uuid::createUuid();
const QString EventDetailsTestBase::kCamera2PhysicalId = "51:dc:54:02:3a:8e";

const QString EventDetailsTestBase::kUser1Name = "User1";
const Uuid EventDetailsTestBase::kUser1Id = Uuid::createUuid();

const QString EventDetailsTestBase::kUser2Name = "User2";
const Uuid EventDetailsTestBase::kUser2Id = Uuid::createUuid();

void EventDetailsTestBase::SetUpTestSuite()
{
    time::Formatter::forceSystemLocale(QLocale::c());

    sContext = std::make_unique<common::SystemContext>(
        common::SystemContext::Mode::unitTests,
        kServerId);
    sEngine = std::make_unique<Engine>(sContext.get(), std::make_unique<TestRouter>());

    auto server = QnResourcePoolTestHelper::createServer(kServerId);
    server->setName(kServerName);
    sContext->resourcePool()->addResource(server);

    auto server2 = QnResourcePoolTestHelper::createServer(kServer2Id);
    server2->setName(kServer2Name);
    sContext->resourcePool()->addResource(server2);

    common::AnalyticsEngineResourcePtr engine1(new common::AnalyticsEngineResource());
    engine1->setIdUnsafe(kEngine1Id);
    engine1->setName(kEngine1Name);
    sContext->resourcePool()->addResource(engine1);

    auto entrance = QnResourcePoolTestHelper::createCamera();
    entrance->setIdUnsafe(kCamera1Id);
    entrance->setName(kCamera1Name);
    entrance->setUrl("10.0.0.1");
    entrance->setPhysicalId(kCamera1PhysicalId);
    sContext->resourcePool()->addResource(entrance);
    entrance->setStatus(nx::vms::api::ResourceStatus::recording);

    auto camera2 = QnResourcePoolTestHelper::createCamera();
    camera2->setIdUnsafe(kCamera2Id);
    camera2->setName(kCamera2Name);
    camera2->setUrl("10.0.0.2");
    camera2->setPhysicalId(kCamera2PhysicalId);
    sContext->resourcePool()->addResource(camera2);
    camera2->setStatus(nx::vms::api::ResourceStatus::recording);

    auto user1 = QnResourcePoolTestHelper::createUser({});
    user1->setIdUnsafe(kUser1Id);
    user1->setName(kUser1Name);
    sContext->resourcePool()->addResource(user1);

    auto user2 = QnResourcePoolTestHelper::createUser({});
    user2->setIdUnsafe(kUser2Id);
    user2->setName(kUser2Name);
    sContext->resourcePool()->addResource(user2);
}

void EventDetailsTestBase::TearDownTestSuite()
{
    sEngine.reset();
    sContext.reset();
    time::Formatter::reset();
}

common::SystemContext* EventDetailsTestBase::systemContext()
{
    return sContext.get();
}

} // namespace nx::vms::rules::test
