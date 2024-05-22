// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QCoreApplication>

#include <nx/branding.h>
#include <nx/vms/client/core/application_context.h>


namespace nx::vms::client::core::test {

/**
 * Helper class that allows to have an application context for a single test.
 */
class UnitTestApplication
{
public:
    UnitTestApplication()
    {
        QCoreApplication::setOrganizationName(nx::branding::company());
        QCoreApplication::setApplicationName("Unit tests");
        m_applicationContext = std::make_unique<ApplicationContext>(
            ApplicationContext::Mode::unitTests,
            nx::vms::api::PeerType::notDefined,
            /*customCloudHost*/ QString{},
            /*ignoreCustomization*/ false
        );
    }

    ~UnitTestApplication()
    {
        QCoreApplication::setOrganizationName(QString());
        QCoreApplication::setApplicationName(QString());
    }

private:
    std::unique_ptr<ApplicationContext> m_applicationContext;
};

} // namespace nx::vms::client::core::test
