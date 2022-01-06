// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QJsonObject>

#include <nx/reflect/instrument.h>

namespace nx::utils {

/** The structure gives a short description of an operating system. */
struct NX_UTILS_API OsInfo
{
    static const QString kDefaultFlavor;

    /** Typically contains the OS family name and architecture: windows_x64, linux_arm32, etc. */
    QString platform;
    /** Used only for Linux. Contains the distribution name: ubuntu, debian, etc. */
    QString variant;
    /** OS or Linux distribution version. */
    QString variantVersion;

    /**
     * A configurable vendor-specific identifier. Should be used to identify a VMS component which
     * receives customized updates.
     */
    QString flavor = kDefaultFlavor;

    OsInfo(
        const QString& platform = {},
        const QString& variant = {},
        const QString& variantVersion = {},
        const QString& flavor = kDefaultFlavor)
        :
        platform(platform),
        variant(variant),
        variantVersion(variantVersion),
        flavor(flavor)
    {
    }

    bool isValid() const { return !platform.isEmpty(); }

    QJsonObject toJson() const;
    static OsInfo fromJson(const QJsonObject& obj);

    QString toString() const;
    static OsInfo fromString(const QString& str);

    bool operator==(const OsInfo& other) const = default;

    static OsInfo current();
    static void overrideVariant(const QString& variant, const QString& variantVersion);
    static void setCurrentFlavor(const QString& flavor);
};
#define OsInfo_Fields (platform)(variant)(variantVersion)(flavor)
NX_REFLECTION_INSTRUMENT(OsInfo, OsInfo_Fields);

NX_UTILS_API QString toString(const OsInfo& info);
NX_UTILS_API size_t qHash(const OsInfo& osInfo, size_t seed = 0);

} // namespace nx::utils
