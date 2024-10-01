// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/api/base_manifest_validation_test.h>
#include <nx/vms/api/analytics/integration_manifest.h>

namespace nx::vms::api::analytics {

class IntegrationManifestValidationTest: public BaseManifestValidationTest<IntegrationManifest>
{
protected:
    void givenCorrectManifest()
    {
        m_manifest = IntegrationManifest();
        m_manifest.id = "testId";
        m_manifest.name = "test name";
        m_manifest.description = "test description";
        m_manifest.version = "test version";
        m_manifest.vendor = "test vendor";
    }

    void givenManifestWithEmptyId()
    {
        givenCorrectManifest();
        m_manifest.id = QString();
    }

    void givenManifestWithEmptyName()
    {
        givenCorrectManifest();
        m_manifest.name = QString();
    }

    void givenManifestWithEmptyDescription()
    {
        givenCorrectManifest();
        m_manifest.description = QString();
    }

    void givenManifestWithEmptyVersion()
    {
        givenCorrectManifest();
        m_manifest.version = QString();
    }

    void givenManifestWithEmptyVendor()
    {
        givenCorrectManifest();
        m_manifest.vendor = QString();
    }

    void givenManifestWithAllFieldsEmpty()
    {
        m_manifest = IntegrationManifest();
    }
};

TEST_F(IntegrationManifestValidationTest, correctManifestProducesNoErrors)
{
    givenCorrectManifest();
    whenValidatingManifest();
    makeSureManifestIsCorrect();
}

TEST_F(IntegrationManifestValidationTest, manifestWithEmptyIdProducesAnError)
{
    givenManifestWithEmptyId();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyIntegrationId});
}

TEST_F(IntegrationManifestValidationTest, manifestWithEmptyNameProducesAnError)
{
    givenManifestWithEmptyName();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyIntegrationName});
}

TEST_F(IntegrationManifestValidationTest, manifestWithEmptyDescriptionProducesAnError)
{
    givenManifestWithEmptyDescription();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyIntegrationDescription});
}

TEST_F(IntegrationManifestValidationTest, manifestWithEmptyVersionProducesAnError)
{
    givenManifestWithEmptyVersion();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyIntegrationVersion});
}

TEST_F(IntegrationManifestValidationTest, DISABLED_manifestWithEmptyVendorProducesAnError)
{
    givenManifestWithEmptyVendor();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyIntegrationVendor});
}

TEST_F(IntegrationManifestValidationTest, allErrorsInEmptyManifestAreCaught)
{
    givenManifestWithAllFieldsEmpty();
    whenValidatingManifest();
    // Note: vendor validation is temporary disabled.
    makeSureErrorsAreCaught({
        ManifestErrorType::emptyIntegrationId,
        ManifestErrorType::emptyIntegrationName,
        ManifestErrorType::emptyIntegrationDescription,
        ManifestErrorType::emptyIntegrationVersion});
}

} // namespace nx::vms::api::analytics
