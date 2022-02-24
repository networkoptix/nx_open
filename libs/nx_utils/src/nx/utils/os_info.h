// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QJsonObject>

#include <nx/reflect/instrument.h>

namespace nx::utils {

struct NX_UTILS_API OsInfo
{
    QString platform;
    QString variant;
    QString variantVersion;

    OsInfo(
        const QString& platform = {},
        const QString& variant = {},
        const QString& variantVersion = {})
        :
        platform(platform),
        variant(variant),
        variantVersion(variantVersion)
    {
    }

    bool isValid() const { return !platform.isEmpty(); }

    QJsonObject toJson() const;
    static OsInfo fromJson(const QJsonObject& obj);

    QString toString() const;
    static OsInfo fromString(const QString& str);

    bool operator==(const OsInfo& other) const;

    static OsInfo current();
    static void override(const QString& variant, const QString& variantVersion);
};
#define OsInfo_Fields (platform)(variant)(variantVersion)
NX_REFLECTION_INSTRUMENT(OsInfo, OsInfo_Fields);

NX_UTILS_API QString toString(const OsInfo& info);
NX_UTILS_API uint qHash(const OsInfo& osInfo, uint seed = 0);

} // namespace nx::utils
