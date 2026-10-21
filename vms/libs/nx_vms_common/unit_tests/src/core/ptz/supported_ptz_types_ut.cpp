// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/ptz/basic_ptz_controller.h>
#include <core/ptz/proxy_ptz_controller.h>
#include <core/ptz/thread_safe_ptz_controller.h>
#include <core/ptz/threaded_ptz_controller.h>

namespace nx::vms::common::ptz::test {
namespace {

class ConfigurablePtzController: public QnBasicPtzController
{
public:
    explicit ConfigurablePtzController(Types types):
        QnBasicPtzController(QnResourcePtr{}),
        m_types(types)
    {
    }

    virtual Types getSupportedPtzTypes() const override { return m_types; }

private:
    Types m_types;
};

} // namespace

TEST(SupportedPtzTypes, basicControllerSupportsNoTypes)
{
    const QnBasicPtzController controller(QnResourcePtr{});
    EXPECT_EQ(controller.getSupportedPtzTypes(), Types{});
    EXPECT_FALSE(controller.supportsPtzType(Type::operational));
    EXPECT_FALSE(controller.supportsPtzType(Type::configurational));
}

TEST(SupportedPtzTypes, concreteModesOnly)
{
    for (const Types types:
        {Types{}, Types(Type::operational), Types(Type::configurational), Types(Type::any)})
    {
        const ConfigurablePtzController controller(types);
        EXPECT_EQ(
            controller.supportsPtzType(Type::operational), types.testFlag(Type::operational));
        EXPECT_EQ(controller.supportsPtzType(Type::configurational),
            types.testFlag(Type::configurational));
        EXPECT_FALSE(controller.supportsPtzType(Type::none));
        EXPECT_FALSE(controller.supportsPtzType(Type::any));
        EXPECT_FALSE(controller.supportsPtzType(static_cast<Type>(4)));
    }
}

TEST(SupportedPtzTypes, proxyFollowsBaseController)
{
    QnProxyPtzController proxy;
    EXPECT_EQ(proxy.getSupportedPtzTypes(), Types{});

    proxy.setBaseController(QnPtzControllerPtr(new ConfigurablePtzController(Type::any)));
    EXPECT_EQ(proxy.getSupportedPtzTypes(), Types(Type::any));

    proxy.setBaseController(
        QnPtzControllerPtr(new ConfigurablePtzController(Type::configurational)));
    EXPECT_EQ(proxy.getSupportedPtzTypes(), Types(Type::configurational));

    proxy.setBaseController({});
    EXPECT_EQ(proxy.getSupportedPtzTypes(), Types{});
}

TEST(SupportedPtzTypes, threadingWrappersPreserveModes)
{
    for (const Types types:
        {Types{}, Types(Type::operational), Types(Type::configurational), Types(Type::any)})
    {
        const QnPtzControllerPtr base(new ConfigurablePtzController(types));
        const QnPtzControllerPtr threaded(new QnThreadedPtzController(base, nullptr));
        const ThreadSafePtzController controller(threaded);
        EXPECT_EQ(controller.getSupportedPtzTypes(), types);
    }
}

} // namespace nx::vms::common::ptz::test
