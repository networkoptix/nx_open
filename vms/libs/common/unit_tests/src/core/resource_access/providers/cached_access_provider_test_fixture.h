#pragma once

#include <deque>

#include <gtest/gtest.h>

#include <core/resource_management/resource_pool_test_helper.h>

#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/providers/abstract_resource_access_provider.h>

class QnCommonModule;
class QnAbstractResourceAccessProvider;

class QnCachedAccessProviderTestFixture: public testing::Test,
    protected QnResourcePoolTestHelper
{
protected:
    virtual void SetUp();
    virtual void TearDown();

    void setupAwaitAccess();
    virtual QnAbstractResourceAccessProvider* accessProvider() const = 0;

    using Source = nx::core::access::Source;

    void awaitAccessValue(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        Source value);

    void at_accessChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        Source value);

private:
    QSharedPointer<QnCommonModule> m_module;

    struct AwaitedAccess
    {
        AwaitedAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
            Source value)
            :
            subject(subject),
            resource(resource),
            value(value)
        {
        }

        QnResourceAccessSubject subject;
        QnResourcePtr resource;
        Source value;
    };
    std::deque<AwaitedAccess> m_awaitedAccessQueue;
};
