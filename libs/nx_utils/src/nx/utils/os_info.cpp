// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_info.h"

#include <QtCore/QStringList>
#include <QtCore/QSysInfo>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <nx/utils/thread/mutex.h>
#include <nx/build_info.h>

namespace nx::utils {

const QString OsInfo::kDefaultFlavor = "default";

QJsonObject OsInfo::toJson() const
{
    return QJsonObject{
        {"platform", platform},
        {"variant", variant},
        {"variantVersion", variantVersion},
        {"flavor", flavor}
    };
}

OsInfo OsInfo::fromJson(const QJsonObject& obj)
{
    return OsInfo{
        obj["platform"].toString(),
        obj["variant"].toString(),
        obj["variantVersion"].toString(),
        obj["flavor"].toString()};
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

// Introduced to make the mutex initialization on-demand.
static nx::ReadWriteLock& mutex()
{
    static nx::ReadWriteLock mtx;
    return mtx;
}

static QString currentVariantOverride;
static QString currentVariantVersionOverride;
static QString currentFlavor;

void OsInfo::overrideVariant(const QString& variant, const QString& variantVersion)
{
    NX_WRITE_LOCKER lock(&mutex());
    currentVariantOverride = variant;
    currentVariantVersionOverride = variantVersion;
}

OsInfo OsInfo::current()
{
    OsInfo result(nx::build_info::applicationPlatformNew());

    {
        NX_READ_LOCKER lock(&mutex());
        result.variant = currentVariantOverride;
        result.variantVersion = currentVariantVersionOverride;
        result.flavor = currentFlavor;
    }

    if (result.variant.isEmpty())
    {
        if (result.platform.startsWith(QStringLiteral("linux")))
            result.variant = QSysInfo::productType();
    }

    if (result.variantVersion.isEmpty())
    {
        result.variantVersion =
            nx::build_info::isWindows() ? QSysInfo::kernelVersion() : QSysInfo::productVersion();
    }

    if (result.flavor.isEmpty())
        result.flavor = kDefaultFlavor;

    return result;
}

void OsInfo::setCurrentFlavor(const QString& flavor)
{
    NX_WRITE_LOCKER lock(&mutex());
    currentFlavor = flavor;
}

QString toString(const OsInfo& info)
{
    QStringList components{info.platform, info.variant, info.variantVersion};
    if (!info.flavor.isEmpty())
        components.append(info.flavor);
    return components.join('-');
}

size_t qHash(const OsInfo& osInfo, size_t seed)
{
    return ::qHash(osInfo.platform + osInfo.variant + osInfo.variantVersion + osInfo.flavor, seed);
}

} // namespace nx::utils
