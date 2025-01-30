// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/test_support/test_context.h>

class QnClientCoreModule;

namespace nx::vms::client::desktop::test {

class MessageProcessorMock;

class Context
{
public:
    Context();
    virtual ~Context();

    SystemContext* systemContext() const;
};

class SystemContextBasedTest: public nx::vms::common::test::GenericContextBasedTest<Context>
{
    using base_type = nx::vms::common::test::GenericContextBasedTest<Context>;

public:
    /** Create and install dummy message processor. */
    MessageProcessorMock* createMessageProcessor();

    /** Client tests should create client layouts. */
    virtual QnLayoutResourcePtr createLayout() override;

    static void initAppContext(ApplicationContext::Features features);
    static void deinitAppContext();

protected:
     virtual void TearDown();
};

struct WithoutFeatures
{
    static ApplicationContext::Features features() { return ApplicationContext::Features::none(); }
};

struct WithQmlFeatures
{
    static ApplicationContext::Features features()
    {
        auto result = ApplicationContext::Features::none();
        result.core.flags.setFlag(core::ApplicationContext::FeatureFlag::qml);
        return result;
    }
};

template<typename FeaturesProvider = WithoutFeatures>
class AppContextBasedTest: public SystemContextBasedTest
{
public:
    static void SetUpTestSuite()
    {
        ApplicationContext::Features features = FeaturesProvider::features();
        initAppContext(features);
    }

    static void TearDownTestSuite()
    {
        deinitAppContext();
    }
};

using ContextBasedTest = AppContextBasedTest<>;
using QmlContextBasedTest = AppContextBasedTest<WithQmlFeatures>;

} // namespace nx::vms::client::desktop::test
