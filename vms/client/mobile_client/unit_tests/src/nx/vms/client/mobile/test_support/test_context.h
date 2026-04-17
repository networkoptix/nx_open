// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/common/test_support/test_context.h>

namespace nx::vms::client::mobile::test {

class Context
{
public:
    Context();
    SystemContext* systemContext() const;
};

class ApplicationContextBasedTest: public common::test::GenericContextBasedTest<Context>
{
    using base_type = common::test::GenericContextBasedTest<Context>;

public:
    /** Client tests should create client layouts. */
    virtual QnLayoutResourcePtr createLayout() const override;

    static void SetUpTestSuite();
    static void TearDownTestSuite();
};

} // namespace nx::vms::client::mobile::test
