#pragma once

#include <deque>

#include <gtest/gtest.h>

#include <core/resource_management/resource_pool_test_helper.h>

#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/providers/abstract_resource_access_provider.h>

class QnCommonModule;
class QnAbstractResourceAccessProvider;

class QnAccessProviderTestFixture: public testing::Test,
    protected QnResourcePoolTestHelper
{
protected:
    virtual void SetUp();
    virtual void TearDown();

    QnAbstractResourceAccessProvider* accessProvider() const;

    void awaitAccessValue(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        QnAbstractResourceAccessProvider::Source value);

    void at_accessChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        QnAbstractResourceAccessProvider::Source value);

    virtual QnAbstractResourceAccessProvider* createAccessProvider() const = 0;

private:
    QSharedPointer<QnCommonModule> m_module;
    QSharedPointer<QnAbstractResourceAccessProvider> m_accessProvider;

    struct AwaitedAccess
    {
        AwaitedAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
            QnAbstractResourceAccessProvider::Source value)
            :
            subject(subject),
            resource(resource),
            value(value)
        {
        }

        QnResourceAccessSubject subject;
        QnResourcePtr resource;
        QnAbstractResourceAccessProvider::Source value;
    };
    std::deque<AwaitedAccess> m_awaitedAccessQueue;
};
