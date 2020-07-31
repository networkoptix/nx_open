#include "os_info.h"

#include <QtCore/QStringList>
#include <QtCore/QSysInfo>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <nx/utils/thread/mutex.h>

#include "app_info.h"

namespace nx::utils {

static nx::ReadWriteLock mutex;
static QString currentVariantOverride;
static QString currentVariantVersionOverride;

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

void OsInfo::override(const QString& variant, const QString& variantVersion)
{
    NX_WRITE_LOCKER lock(&mutex);
    currentVariantOverride = variant;
    currentVariantVersionOverride = variantVersion;
}

OsInfo OsInfo::current()
{
    OsInfo result(AppInfo::applicationPlatformNew());
    {
        NX_READ_LOCKER lock(&mutex);
        result.variant = currentVariantOverride;
        result.variantVersion = currentVariantVersionOverride;
    }
    if (result.variant.isEmpty())
    {
        if (result.platform.startsWith(QStringLiteral("linux")))
            result.variant = QSysInfo::productType();
    }
    if (result.variantVersion.isEmpty())
    {
        result.variantVersion =
            AppInfo::isWindows() ? QSysInfo::kernelVersion() : QSysInfo::productVersion();
    }
    return result;
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
