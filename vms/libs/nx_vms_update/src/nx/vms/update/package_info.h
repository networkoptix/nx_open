// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/os_info.h>
#include <nx/vms/api/data/software_version_serialization.h>

namespace nx::vms::update {

struct NX_VMS_UPDATE_API PlatformVariant
{
    QString name;
    QString minimumVersion = {};
    QString maximumVersion = {};

    bool operator==(const PlatformVariant& other) const = default;
};
#define PlatformVariant_Fields (name)(minimumVersion)(maximumVersion)
NX_REFLECTION_INSTRUMENT(PlatformVariant, PlatformVariant_Fields)
QN_FUSION_DECLARE_FUNCTIONS(PlatformVariant, (hash)(json), NX_VMS_UPDATE_API)

using PlatformVariantList = QVector<PlatformVariant>;

NX_REFLECTION_ENUM_CLASS(Component,
    unknown,
    client,
    customClient,
    server
)

inline size_t qHash(Component component, size_t seed = 0)
{
    return ::qHash((uint) (component), seed);
}

struct NX_VMS_UPDATE_API PackageInfo
{
    nx::utils::SoftwareVersion version;
    Component component;
    QString cloudHost;
    QString platform;
    QString customClientVariant;
    PlatformVariantList variants;
    QString installScript;
    qint64 freeSpaceRequired = 0;

    bool isValid() const;
    bool isCompatibleTo(const nx::utils::OsInfo& osInfo, bool ignoreVersion = false) const;
};
#define PackageInfo_Fields \
    (version) \
    (component) \
    (cloudHost) \
    (platform) \
    (customClientVariant) \
    (variants) \
    (installScript) \
    (freeSpaceRequired)
NX_REFLECTION_INSTRUMENT(PackageInfo, PackageInfo_Fields)
QN_FUSION_DECLARE_FUNCTIONS(PackageInfo, (json), NX_VMS_UPDATE_API)

NX_VMS_UPDATE_API bool isPackageCompatibleTo(
    const QString& packagePlatform,
    const PlatformVariantList& packageVariants,
    const nx::utils::OsInfo& osInfo,
    bool ignoreVersion = false);

NX_VMS_UPDATE_API bool isPackageNewerForVariant(
    const QString& variant,
    const PlatformVariantList& packageVariants,
    const PlatformVariantList& otherPackageVariants);

} // namespace nx::vms::update
