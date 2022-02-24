// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/build_info/ini.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/reflect/string_conversion.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx_ec/ec_api_common.h>

namespace nx::vms::common {

using DeveloperFlag = ServerCompatibilityValidator::DeveloperFlag;
using DeveloperFlags = ServerCompatibilityValidator::DeveloperFlags;
using Peer = ServerCompatibilityValidator::Peer;
using Protocol = ServerCompatibilityValidator::Protocol;
using PublicationType = nx::build_info::PublicationType;
using Reason = ServerCompatibilityValidator::Reason;

void PrintTo(Reason reason, ::std::ostream* os)
{
    static const QMap<Reason, std::string> kReasonToString{
        {Reason::binaryProtocolVersionDiffers, "binaryProtocolVersionDiffers"},
        {Reason::cloudHostDiffers, "cloudHostDiffers"},
        {Reason::customizationDiffers, "customizationDiffers"},
        {Reason::versionIsTooLow, "versionIsTooLow"},
    };
    *os << kReasonToString[reason];
}

namespace test {

class ConnectionValidatorTest: public ::testing::Test
{
private:
    nx::vms::api::ModuleInformation serverModuleInformation() const
    {
        nx::vms::api::ModuleInformation moduleInformation;
        moduleInformation.customization = m_customization;
        moduleInformation.protoVersion = m_protocolVersion;
        moduleInformation.version = m_version;
        moduleInformation.cloudHost = m_cloudHost;
        return moduleInformation;
    }

protected:
    void givenDesktopClient()
    {
        m_validatorLabel = "Desktop Client";
        ServerCompatibilityValidator::initialize(Peer::desktopClient);
    }

    void givenMobileClient()
    {
        m_validatorLabel = "Mobile Client";
        ServerCompatibilityValidator::initialize(Peer::mobileClient);
    }

    void givenCompatibilityModeClient()
    {
        m_validatorLabel = "Compatibility Mobile Client";
        ServerCompatibilityValidator::initialize(
            Peer::desktopClient,
            Protocol::json,
            {DeveloperFlag::ignoreCustomization});
    }

    void givenRemoteServer()
    {
        m_protocolVersion = nx::vms::api::protocolVersion();
        m_customization = nx::branding::customization();
        m_version = nx::utils::SoftwareVersion{nx::build_info::vmsVersion()};
        m_cloudHost = QString::fromStdString(nx::network::SocketGlobals::cloud().cloudHost());
    }

    void whenProtocolVersionDiffers()
    {
        static const int kIncompatibleProtocolVersion = nx::vms::api::protocolVersion() + 1;
        m_protocolVersion = kIncompatibleProtocolVersion;
    }

    void whenCloudHostDiffers()
    {
        static const QString kIncompatibleCloudHost = "incompatible_cloud_host";
        m_cloudHost = kIncompatibleCloudHost;
    }

    void whenClientPublicationTypeIs(PublicationType value)
    {
        if (!iniTweaks)
            iniTweaks = std::make_unique<nx::kit::IniConfig::Tweaks>();

        m_publicationType = std::make_unique<std::string>(nx::reflect::toString(value));
        iniTweaks->set(&nx::build_info::ini().publicationType, m_publicationType->c_str());
    }

    void thenConnectionIsCompatible()
    {
        const auto reason = ServerCompatibilityValidator::check(serverModuleInformation());
        ASSERT_FALSE(reason.has_value());
    }

    void thenConnectionIncompatibleBy(Reason expected)
    {
        const auto reason = ServerCompatibilityValidator::check(serverModuleInformation());
        ASSERT_TRUE(reason.has_value());
        ASSERT_EQ(*reason, expected) << m_validatorLabel;
    }

private:
    std::string m_validatorLabel = "Unknown";
    int m_protocolVersion = 0;
    QString m_customization;
    nx::vms::api::SoftwareVersion m_version;
    QString m_cloudHost;
    std::unique_ptr<std::string> m_publicationType;
    std::unique_ptr<nx::kit::IniConfig::Tweaks> iniTweaks;
};

TEST_F(ConnectionValidatorTest, protocolVersionCheck)
{
    givenRemoteServer();
    whenProtocolVersionDiffers();

    givenDesktopClient();
    thenConnectionIncompatibleBy(Reason::binaryProtocolVersionDiffers);

    givenMobileClient();
    thenConnectionIsCompatible();

    givenCompatibilityModeClient();
    thenConnectionIsCompatible();
}

TEST_F(ConnectionValidatorTest, nonReleaseMobileClientIgnoresCloudHost)
{
    givenRemoteServer();
    whenCloudHostDiffers();
    whenClientPublicationTypeIs(PublicationType::release);

    givenMobileClient();
    thenConnectionIncompatibleBy(Reason::cloudHostDiffers);

    givenDesktopClient();
    thenConnectionIncompatibleBy(Reason::cloudHostDiffers);
}

TEST_F(ConnectionValidatorTest, releaseMobileClientRespectsCloudHost)
{
    givenRemoteServer();
    whenCloudHostDiffers();
    whenClientPublicationTypeIs(PublicationType::private_build);

    givenMobileClient();
    thenConnectionIsCompatible();

    givenDesktopClient();
    thenConnectionIncompatibleBy(Reason::cloudHostDiffers);
}

} // namespace test
} // namespace nx::vms::common
