// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/providers/direct_access_provider_test_fixture.h>

namespace nx::core::access {
namespace test {

class DirectBaseAccessProviderTestFixture: public DirectAccessProviderTestFixture
{
    using base_type = DirectAccessProviderTestFixture;

protected:
    virtual void SetUp() override;
    virtual void TearDown() override;

    virtual AbstractResourceAccessProvider* accessProvider() const override;
    virtual AbstractResourceAccessProvider* createAccessProvider() const = 0;

private:
    AbstractResourceAccessProvider* m_accessProvider = nullptr;
};

} // namespace test
} // namespace nx::core::access
