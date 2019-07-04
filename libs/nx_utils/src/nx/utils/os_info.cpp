#include "os_info.h"

#include <QtCore/QStringList>
#include <QtCore/QSysInfo>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include "app_info.h"

namespace nx::utils {

QString OsInfo::currentVariantOverride;
QString OsInfo::currentVariantVersionOverride;

QJsonObject OsInfo::toJson() const
{
    return QJsonObject{
        {"platform", platform},
        {"variant", variant},
        {"variantVersion", variantVersion}
    };
}

OsInfo OsInfo::fromJson(const QJsonObject& obj)
{
    return OsInfo{
        obj["platform"].toString(),
        obj["variant"].toString(),
        obj["variantVersion"].toString()};
}

QString OsInfo::toString() const
{
    return QString::fromLatin1(QJsonDocument(toJson()).toJson(QJsonDocument::Compact));
}

OsInfo OsInfo::fromString(const QString& str)
{
    const QJsonObject& obj = QJsonDocument::fromJson(str.toLatin1()).object();
    if (obj.isEmpty())
        return {};

    return fromJson(obj);
}

bool OsInfo::operator==(const OsInfo& other) const
{
    return platform == other.platform
        && variant == other.variant
        && variantVersion == other.variantVersion;
}

QString OsInfo::currentPlatform()
{
    return AppInfo::applicationPlatformNew();
}

QString OsInfo::currentVariant()
{
    if (!currentVariantOverride.isEmpty())
        return currentVariantOverride;

    if (currentPlatform().startsWith(QStringLiteral("linux")))
        return QSysInfo::productType();
    return {};
}

QString OsInfo::currentVariantVersion()
{
    if (!currentVariantVersionOverride.isEmpty())
        return currentVariantVersionOverride;

    if (AppInfo::isWindows())
        return QSysInfo::kernelVersion();
    return QSysInfo::productVersion();
}

OsInfo OsInfo::current()
{
    return OsInfo(currentPlatform(), currentVariant(), currentVariantVersion());
}

QString toString(const OsInfo& info)
{
    return QStringList{info.platform, info.variant, info.variantVersion}.join(L'-');
}

uint qHash(const OsInfo& osInfo, uint seed)
{
    return ::qHash(osInfo.platform + osInfo.variant + osInfo.variantVersion, seed);
}

} // namespace nx::utils
