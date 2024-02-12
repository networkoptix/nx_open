// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtTest/QSignalSpy>

#include <core/resource_access/access_rights_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/test_context.h>

namespace nx::core::access {
namespace test {

class ResourceAccessResolverTestFixture: public nx::vms::common::test::ContextBasedTest
{
public:
    using Notifier = AbstractResourceAccessResolver::Notifier;

    ResourceAccessResolverTestFixture()
    {
        qRegisterMetaType<QSet<nx::Uuid>>();
    }

    virtual AbstractResourceAccessResolver* createResolver() const = 0;

    virtual void SetUp() override
    {
        manager.reset(new AccessRightsManager());
        resolver.reset(createResolver());
        resolver->notifier()->subscribeSubjects({kTestSubjectId});

        resourceAccessReset.reset(new QSignalSpy(resolver->notifier(),
            &AbstractResourceAccessResolver::Notifier::resourceAccessReset));

        resourceAccessChanged.reset(new QSignalSpy(resolver->notifier(),
            &AbstractResourceAccessResolver::Notifier::resourceAccessChanged));
    }

    virtual void TearDown() override
    {
        resourceAccessReset.reset();
        resourceAccessChanged.reset();
        resolver.reset();
        manager.reset();
        ContextBasedTest::TearDown();
    }

public:
    const nx::Uuid kTestSubjectId = nx::Uuid::createUuid();
    std::unique_ptr<AccessRightsManager> manager;
    std::unique_ptr<AbstractResourceAccessResolver> resolver;
    std::unique_ptr<QSignalSpy> resourceAccessReset;
    std::unique_ptr<QSignalSpy> resourceAccessChanged;
};

#define NX_ASSERT_NO_SIGNAL(signalSpy) \
    ASSERT_TRUE(signalSpy->empty());

#define NX_ASSERT_SIGNAL(signalSpy) \
    ASSERT_EQ(signalSpy->size(), 1); \
    signalSpy->clear();

#define NX_ASSERT_SIGNALS(signalSpy, count) \
    ASSERT_EQ(signalSpy->size(), count); \
    signalSpy->clear();

#define NX_ASSERT_SIGNAL_ARGS(signalSpy, subjectIds) { \
    ASSERT_EQ(signalSpy->size(), 1); \
    const auto args = signalSpy->takeFirst(); \
    ASSERT_EQ(args.size(), 1); \
    ASSERT_EQ(args[0].value<QSet<nx::Uuid>>(), subjectIds); }

#define NX_ASSERT_TEST_SUBJECT_CHANGED() \
    NX_ASSERT_SIGNAL_ARGS(resourceAccessChanged, QSet<nx::Uuid>({kTestSubjectId}))

} // namespace test
} // namespace nx::core::access
