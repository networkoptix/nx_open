// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QString>
#include <QtCore/QVector>

#include <nx/build_info.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/software_version.h>

namespace nx::vms::update {

using PublicationType = nx::build_info::PublicationType;

NX_REFLECTION_ENUM_CLASS(Product,
    vms,
    desktop_client
)

struct NX_VMS_UPDATE_API ReleaseInfo
{
    nx::vms::api::SoftwareVersion version;
    Product product;
    int protocol_version = 0;
    PublicationType publication_type = PublicationType::release;
    qint64 release_date = 0;
    int release_delivery_days = 0;
};
#define ReleaseInfo_Fields \
    (version) \
    (product) \
    (protocol_version) \
    (publication_type) \
    (release_date) \
    (release_delivery_days)


struct NX_VMS_UPDATE_API ReleasesInfo
{
    QVector<nx::utils::Url> packages_urls;
    QVector<ReleaseInfo> releases;

    std::optional<ReleaseInfo> selectVmsRelease(
        const nx::utils::SoftwareVersion& currentVersion) const;
    std::optional<ReleaseInfo> selectDesktopClientRelease(
        const nx::utils::SoftwareVersion& currentVersion,
        const PublicationType publicationType,
        const int protocolVersion) const;
};
#define ReleasesInfo_Fields (packages_urls)(releases)
QN_FUSION_DECLARE_FUNCTIONS(ReleasesInfo, (json), NX_VMS_UPDATE_API)

} // namespace nx::vms::update
