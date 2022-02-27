// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "update_check_params.h"

#include <nx/reflect/string_conversion.h>

namespace nx::vms::common::update {

using namespace nx::vms::update;

UpdateInfoParams::UpdateInfoParams(const vms::update::PublicationInfoParams& params)
{
    if (const auto p = std::get_if<LatestVmsVersionParams>(&params))
        *this = *p;
    if (const auto p = std::get_if<LatestDesktopClientVersionParams>(&params))
        *this = *p;
    if (const auto p = std::get_if<CertainVersionParams>(&params))
        *this = *p;
}

std::optional<vms::update::PublicationInfoParams> UpdateInfoParams::toPublicationInfoParams() const
{
    if (const auto p = std::get_if<LatestVmsVersionParams>(this))
        return *p;
    if (const auto p = std::get_if<LatestDesktopClientVersionParams>(this))
        return *p;
    if (const auto p = std::get_if<CertainVersionParams>(this))
        return *p;

    return {};
}

QMultiMap<QString, QString> UpdateInfoParams::toRestParams() const
{
    if (std::holds_alternative<TargetVersionParams>(*this))
        return {{"version", "target"}};
    if (std::holds_alternative<InstalledVersionParams>(*this))
        return {{"version", "installed"}};

    if (const auto& params = std::get_if<LatestVmsVersionParams>(this))
    {
        return {
            {"product", "vms"},
            {"current_version", params->currentVersion.toString()}
        };
    }
    if (const auto& params = std::get_if<LatestDesktopClientVersionParams>(this))
    {
        return {
            {"product", "desktop_client"},
            {"publication_type",
                QString::fromStdString(nx::reflect::toString(params->publicationType))},
            {"current_version", params->currentVersion.toString()},
            {"protocol_version", QString::number(params->protocolVersion)},
        };
    }
    if (const auto& params = std::get_if<CertainVersionParams>(this))
    {
        return {
            {"product", "vms"},
            {"version", params->version.toString()},
        };
    }

    return {};
}

UpdateInfoParams UpdateInfoParams::fromRestParams(const QMultiMap<QString, QString>& params)
{
    const QString& product = params.value("product");
    if (product == "desktop_client")
    {
        return LatestDesktopClientVersionParams{
            nx::utils::SoftwareVersion(params.value("current_version")),
            nx::reflect::fromString<PublicationType>(
                params.value("publication_type").toStdString()),
            params.value("protocol_version").toInt()
        };
    }

    const QString version = params.value("version");

    if (version == kTargetKey)
        return TargetVersionParams{};
    if (version == kInstalledKey)
        return InstalledVersionParams{};

    if (version.isEmpty())
    {
        const nx::utils::SoftwareVersion currentVersion(params.value("current_version"));
        if (currentVersion.isNull())
            return TargetVersionParams{};

        return LatestVmsVersionParams{currentVersion};
    }

    return CertainVersionParams{nx::utils::SoftwareVersion(version)};
}

} // namespace nx::vms::common::update
