#pragma once

#include <QtCore/QString>
#include <QtCore/QJsonObject>

namespace nx::utils {

class NX_UTILS_API OsInfo
{
public:
    QString platform;
    QString variant;
    QString variantVersion;

public:
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

NX_UTILS_API QString toString(const OsInfo& info);
NX_UTILS_API uint qHash(const OsInfo& osInfo, uint seed = 0);

} // namespace nx::utils
