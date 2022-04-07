// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/utils/software_version.h>
#include <nx_ec/ec_api_fwd.h>

class QnCommonModule;

namespace nx::vms::api {

struct ModuleInformation;
struct SystemInformation;
class SoftwareVersion;

} // namespace nx::vms::api

namespace nx::vms::common {

/**
 * Helper class to diagnose if the target Server compatible to current System or to the Client.
 * There are following incompatibility reasons:
 *   * If the Server has different customization
 *   * If the Server has different cloud host
 *   * If Server version is too low
 *   * If binary protocol is used, and its version differs
 */
class NX_VMS_COMMON_API ServerCompatibilityValidator
{
public:
    enum class Purpose
    {
        connect,
        merge,
        connectInCompatibilityMode,
    };

    enum class Reason
    {
        binaryProtocolVersionDiffers,
        cloudHostDiffers,
        customizationDiffers,
        versionIsTooLow,
    };

    enum class Peer
    {
        undefined,
        server,
        mobileClient,
        desktopClient,
    };

    enum class Protocol
    {
        undefined,
        autoDetect,
        ubjson,
        json,
    };

    enum class DeveloperFlag
    {
        empty = 0,
        ignoreCustomization = 0x1,
        ignoreCloudHost = 0x2,
    };
    Q_DECLARE_FLAGS(DeveloperFlags, DeveloperFlag)

    static void initialize(
        Peer localPeerType,
        Protocol connectionProtocol = Protocol::autoDetect,
        DeveloperFlags developerFlags = {DeveloperFlag::empty});

    static std::optional<Reason> check(
        const nx::vms::api::SystemInformation& systemInformation,
        Purpose purpose = Purpose::connect);

    static std::optional<Reason> check(
        const nx::vms::api::ModuleInformation& moduleInformation,
        Purpose purpose = Purpose::connect);

    static bool isCompatible(
        const nx::vms::api::SystemInformation& systemInformation,
        Purpose purpose = Purpose::connect);

    static bool isCompatible(
        const nx::vms::api::ModuleInformation& moduleInformation,
        Purpose purpose = Purpose::connect);

    static nx::utils::SoftwareVersion minimalVersion(Purpose purpose = Purpose::connect);

    /**
     * Check customization compatiblity. Servers and desktop client customization must be equal,
     * mobile client supports own customization and a special compatibility list.
     */
    static bool isCompatibleCustomization(const QString& remoteCustomization);
};

} // namespace nx::vms::common
