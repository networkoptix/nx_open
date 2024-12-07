// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <variant>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QVector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/os_info.h>
#include <nx/utils/url.h>

#include "package_info.h"

namespace nx::vms::update {

struct NX_VMS_UPDATE_API Package
{
    Component component;

    QString platform;
    QString customClientVariant;
    PlatformVariantList variants;

    QString file;
    QString md5;
    qint64 size = 0;
    QByteArray signature;

    nx::utils::Url url;

    bool operator==(const Package& other) const = default;

    bool isValid() const;

    bool isCompatibleTo(const nx::utils::OsInfo& osInfo, bool ignoreVersion = false) const;
    bool isNewerThan(const QString& variant, const Package& other) const;
    bool isSameTarget(const Package& other) const;
};
#define Package_Fields \
    (component)(platform)(customClientVariant)(variants)(file)(size)(md5)(url)(signature)

NX_REFLECTION_INSTRUMENT(Package, Package_Fields)
QN_FUSION_DECLARE_FUNCTIONS(Package, (hash)(json), NX_VMS_UPDATE_API)

NX_REFLECTION_ENUM_CLASS(FindPackageError,
    notFound,
    osVersionNotSupported
)

struct NX_VMS_UPDATE_API PublicationInfo
{
    nx::utils::SoftwareVersion version;
    QString cloudHost;
    QString eulaLink;
    int eulaVersion = 0;
    QString releaseNotesUrl;

    /** Release date in msecs since epoch. */
    std::chrono::milliseconds releaseDate{0};
    /** Maximum days for release delivery. */
    int releaseDeliveryDays = 0;

    /**
     * Release notes for offline update package.
     * We need to be able to show this data without internet.
     */
    QString description;
    QString eula;
    QList<Package> packages;

    nx::utils::Url url;

    using FindPackageResult = std::variant<Package, FindPackageError>;

    FindPackageResult findPackage(
        Component component,
        const nx::utils::OsInfo& osInfo,
        const QString& customClientVariant = {}) const;
};
#define PublicationInfo_Fields \
    (version) \
    (cloudHost) \
    (eulaLink) \
    (eulaVersion) \
    (releaseNotesUrl) \
    (releaseDate) \
    (releaseDeliveryDays) \
    (description) \
    (eula) \
    (packages) \
    (url)

NX_REFLECTION_INSTRUMENT(PublicationInfo, PublicationInfo_Fields)
QN_FUSION_DECLARE_FUNCTIONS(PublicationInfo, (json), NX_VMS_UPDATE_API)

} // namespace nx::vms::update
