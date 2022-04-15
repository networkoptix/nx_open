// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <gtest/gtest.h>

#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource_access/providers/abstract_resource_access_provider.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/common/test_support/test_context.h>

class AbstractResourceAccessProvider;

namespace nx::core::access {
namespace test {

class DirectAccessProviderTestFixture: public nx::vms::common::test::ContextBasedTest
{
protected:
    virtual AbstractResourceAccessProvider* accessProvider() const = 0;
};

} // namespace test
} // namespace nx::core::access
