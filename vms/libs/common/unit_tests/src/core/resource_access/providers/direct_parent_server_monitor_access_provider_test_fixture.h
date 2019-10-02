#pragma once

#include <core/resource_access/providers/direct_access_provider_test_fixture.h>

class QnDirectParentServerMonitorAccessProviderTest: public QnDirectAccessProviderTestFixture
{
    using base_type = QnDirectAccessProviderTestFixture;

protected:
    virtual QnAbstractResourceAccessProvider* accessProvider() const override;
};
