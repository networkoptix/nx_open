#pragma once

#include <core/resource_access/providers/direct_access_provider_test_fixture.h>

class QnDirectBaseAccessProviderTestFixture: public QnDirectAccessProviderTestFixture
{
    using base_type = QnDirectAccessProviderTestFixture;

protected:
    virtual void SetUp() override;
    virtual void TearDown() override;

    virtual QnAbstractResourceAccessProvider* accessProvider() const override;
    virtual QnAbstractResourceAccessProvider* createAccessProvider() const = 0;

private:
    QnAbstractResourceAccessProvider* m_accessProvider = nullptr;
};
