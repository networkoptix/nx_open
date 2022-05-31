// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/core/access/access_types.h>
#include <nx/utils/impl_ptr.h>

class QnClientCoreModule;
class QnCommonModule;

namespace nx::vms::client::desktop { class SystemContext; }

namespace nx::vms::client::desktop::test {

class Context
{
public:
    Context();
    virtual ~Context();

    QnCommonModule* commonModule() const;
    QnClientCoreModule* clientCoreModule() const;
    SystemContext* systemContext() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

class ContextBasedTest: public ::testing::Test
{
public:
    ContextBasedTest()
    {
        m_context = std::make_unique<Context>();
    }

    ~ContextBasedTest()
    {
        m_context.reset();
    }

    Context* context() const { return m_context.get(); }

    QnCommonModule* commonModule() const { return m_context->commonModule(); }
    SystemContext* systemContext() const { return m_context->systemContext(); }

private:
    std::unique_ptr<Context> m_context;
};

} // namespace nx::vms::client::desktop::test
