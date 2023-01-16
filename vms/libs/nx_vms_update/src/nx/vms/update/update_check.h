// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>
#include <variant>
#include <utility>

#include <QtCore/QString>
#include <QtCore/QMultiMap>

#include <nx/utils/os_info.h>
#include <nx/utils/url.h>
#include <nx/utils/software_version.h>

#include "releases_info.h"
#include "publication_info.h"

namespace nx::vms::update {

enum class FetchError
{
    networkError,
    httpError,
    parseError,
    paramsError,
};

using FetchReleasesResult = std::variant<ReleasesInfo, FetchError>;

NX_VMS_UPDATE_API FetchReleasesResult fetchReleasesInfo(
    const nx::utils::Url& url);

using FetchPublicationInfoResult = std::variant<PublicationInfo, FetchError>;

NX_VMS_UPDATE_API FetchPublicationInfoResult fetchPublicationInfo(
    const nx::utils::SoftwareVersion& version, const nx::utils::Url& urlPrefix);
NX_VMS_UPDATE_API FetchPublicationInfoResult fetchPublicationInfo(
    const nx::utils::SoftwareVersion& version, const QVector<nx::utils::Url>& urlPrefixes);

NX_VMS_UPDATE_API FetchPublicationInfoResult fetchLegacyPublicationInfo(
    const nx::utils::SoftwareVersion& version, const nx::utils::Url& urlPrefix);
NX_VMS_UPDATE_API FetchPublicationInfoResult fetchLegacyPublicationInfo(
    const nx::utils::SoftwareVersion& version, const QVector<nx::utils::Url>& urlPrefixes);

using PublicationInfoResult = std::variant<PublicationInfo, FetchError, std::nullptr_t>;

struct NX_VMS_UPDATE_API LatestVmsVersionParams
{
    nx::utils::SoftwareVersion currentVersion;
};
struct NX_VMS_UPDATE_API LatestDesktopClientVersionParams
{
    nx::utils::SoftwareVersion currentVersion;
    PublicationType publicationType;
    int protocolVersion;
};
struct NX_VMS_UPDATE_API CertainVersionParams
{
    nx::utils::SoftwareVersion version;
    nx::utils::SoftwareVersion currentVersion;
};

using PublicationInfoParams = std::variant<
    LatestVmsVersionParams,
    LatestDesktopClientVersionParams,
    CertainVersionParams>;

NX_VMS_UPDATE_API PublicationInfoResult getPublicationInfo(
    const nx::utils::Url& url,
    const PublicationInfoParams& params);

} // namespace nx::vms::update
