#pragma once

#include <core/resource_access/providers/cached_access_provider_test_fixture.h>

class QnCachedBaseAccessProviderTestFixture: public QnCachedAccessProviderTestFixture
{
    using base_type = QnCachedAccessProviderTestFixture;

protected:
    virtual void SetUp() override;
    virtual void TearDown() override;

    virtual QnAbstractResourceAccessProvider* accessProvider() const override;
    virtual QnAbstractResourceAccessProvider* createAccessProvider() const = 0;

private:
    QnAbstractResourceAccessProvider* m_accessProvider = nullptr;
};
