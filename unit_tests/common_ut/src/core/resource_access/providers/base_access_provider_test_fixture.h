#pragma once

#include <core/resource_access/providers/access_provider_test_fixture.h>

class QnBaseAccessProviderTestFixture: public QnAccessProviderTestFixture
{
    using base_type = QnAccessProviderTestFixture;

protected:
    virtual void SetUp() override;
    virtual void TearDown() override;

    virtual QnAbstractResourceAccessProvider* accessProvider() const override;
    virtual QnAbstractResourceAccessProvider* createAccessProvider() const = 0;

private:
    QnAbstractResourceAccessProvider* m_accessProvider = nullptr;
};
