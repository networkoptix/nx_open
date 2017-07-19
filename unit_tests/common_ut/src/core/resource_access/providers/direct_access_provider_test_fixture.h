#pragma once

#include <deque>

#include <gtest/gtest.h>

#include <core/resource_management/resource_pool_test_helper.h>

#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/providers/abstract_resource_access_provider.h>

class QnCommonModule;
class QnAbstractResourceAccessProvider;

class QnDirectAccessProviderTestFixture: public testing::Test,
    protected QnResourcePoolTestHelper
{
protected:
    virtual void SetUp();
    virtual void TearDown();

    virtual QnAbstractResourceAccessProvider* accessProvider() const = 0;

private:
    QSharedPointer<QnCommonModule> m_module;
};
