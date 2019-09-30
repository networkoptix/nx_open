#include <gtest/gtest.h>

#include <nx/vms/api/base_manifest_validation_test.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

namespace nx::vms::api::analytics {

class PluginManifestValidationTest: public BaseManifestValidationTest<PluginManifest>
{
protected:
    void givenCorrectManifest()
    {
        m_manifest = PluginManifest();
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
        m_manifest = PluginManifest();
    }
};

TEST_F(PluginManifestValidationTest, correctManifestProducesNoErrors)
{
    givenCorrectManifest();
    whenValidatingManifest();
    makeSureManifestIsCorrect();
}

TEST_F(PluginManifestValidationTest, manifestWithEmptyIdProducesAnError)
{
    givenManifestWithEmptyId();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyPluginId});
}

TEST_F(PluginManifestValidationTest, manifestWithEmptyNameProducesAnError)
{
    givenManifestWithEmptyName();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyPluginName});
}

TEST_F(PluginManifestValidationTest, manifestWithEmptyDescriptionProducesAnError)
{
    givenManifestWithEmptyDescription();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyPluginDescription});
}

TEST_F(PluginManifestValidationTest, manifestWithEmptyVersionProducesAnError)
{
    givenManifestWithEmptyVersion();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyPluginVersion});
}

TEST_F(PluginManifestValidationTest, DISABLED_manifestWithEmptyVendorProducesAnError)
{
    givenManifestWithEmptyVendor();
    whenValidatingManifest();
    makeSureErrorsAreCaught({ManifestErrorType::emptyPluginVendor});
}

TEST_F(PluginManifestValidationTest, allErrorsInEmptyManifestAreCaught)
{
    givenManifestWithAllFieldsEmpty();
    whenValidatingManifest();
    // Note: vendor validation is temporary disabled.
    makeSureErrorsAreCaught({
        ManifestErrorType::emptyPluginId,
        ManifestErrorType::emptyPluginName,
        ManifestErrorType::emptyPluginDescription,
        ManifestErrorType::emptyPluginVersion});
}

} // namespace nx::vms::api::analytics
