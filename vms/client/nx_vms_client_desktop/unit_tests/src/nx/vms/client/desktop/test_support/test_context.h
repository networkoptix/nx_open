// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/utils/impl_ptr.h>
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

    QnClientCoreModule* clientCoreModule() const;
    SystemContext* systemContext() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

class ContextBasedTest: public nx::vms::common::test::GenericContextBasedTest<Context>
{
public:
    /** Create and install dummy message processor. */
    MessageProcessorMock* createMessageProcessor();

    /** Client tests should create client layouts. */
    virtual QnLayoutResourcePtr createLayout() override;
};

} // namespace nx::vms::client::desktop::test
