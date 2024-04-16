// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_icon_manager.h"

#include <QtCore/QCache>
#include <QtCore/QFileInfo>
#include <QtQml/QtQml>
#include <QtQml/QQmlEngine>

#include <nx/utils/log/log.h>

namespace nx::vms::client::core {
namespace analytics {

struct IconManager::Private
{
    IconManager* const q;
    mutable QCache<QString, QString> pathCache{};

    QString fallbackIconPath() const;
    QString calculateIconPath(const QString& iconName) const;
    void splitIconName(const QString& iconName, QString& libraryName, QString& imageName) const;
    QString getExtension(const QString& libraryName) const;

    static QString analyticsIconsRoot();
};

IconManager::IconManager(): d(new Private{this})
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

IconManager::~IconManager()
{
    // Required here for forward-declared scoped pointer destruction.
}

IconManager* IconManager::instance()
{
    static IconManager instance;
    return &instance;
}

void IconManager::registerQmlType()
{
    qmlRegisterSingletonInstance<IconManager>("nx.vms.client.core.analytics", 1, 0,
        "IconManager", IconManager::instance());
}

QString IconManager::skinRoot()
{
    static const QString kSkinRoot = ":/skin/";
    return kSkinRoot;
}

QString IconManager::librariesRoot()
{
    static const QString kLibrariesRootUrl = skinRoot() + Private::analyticsIconsRoot();
    return kLibrariesRootUrl;
}

QString IconManager::iconPath(const QString& iconName) const
{
    const QString* cachedPath = d->pathCache.object(iconName);
    if (cachedPath)
        return *cachedPath;

    const QString calculatedPath = d->calculateIconPath(iconName);
    d->pathCache.insert(iconName, new QString(calculatedPath));
    return calculatedPath;
}

QString IconManager::fallbackIconPath() const
{
    return d->fallbackIconPath();
}

QString IconManager::absoluteIconPath(const QString& iconName) const
{
    return skinRoot() + iconPath(iconName);
}

QString IconManager::iconUrl(const QString& iconName) const
{
    static const QString kSvgSkinRootUrl = "image://skin/";
    static const QString kDefaultSkinRootUrl = "qrc:///skin/";

    const auto relativePath = iconPath(iconName);
    return relativePath.endsWith("svg", Qt::CaseInsensitive)
        ? kSvgSkinRootUrl + relativePath
        : kDefaultSkinRootUrl + relativePath;
}

QString IconManager::Private::calculateIconPath(const QString& iconName) const
{
    if (iconName.isEmpty())
        return fallbackIconPath();

    QString libraryName;
    QString imageName;
    splitIconName(iconName, libraryName, imageName);

    if (libraryName.isEmpty() || imageName.isEmpty())
    {
        NX_WARNING(q, "Ill-formed icon name \"%1\"", iconName);
        return fallbackIconPath();
    }

    const QString libraryPath = analyticsIconsRoot() + libraryName;
    if (!QFileInfo::exists(skinRoot() + libraryPath))
    {
        NX_WARNING(q, "Unknown icon library \"%1\"", libraryName);
        return fallbackIconPath();
    }

    const QString imageFileName = imageName + getExtension(libraryName);
    const QString imagePath = libraryPath + "/" + imageFileName;

    if (!QFileInfo::exists(skinRoot() + imagePath))
    {
        NX_WARNING(q, "Icon \"%1\" is not found in the library \"%2\"", imageFileName,
            libraryName);
        return fallbackIconPath();
    }

    return imagePath;
}

QString IconManager::Private::fallbackIconPath() const
{
    static const QString kDefaultIconPath = "analytics_icons/nx.base/default.svg";
    return kDefaultIconPath;
}

void IconManager::Private::splitIconName(
    const QString& iconName, QString& libraryName, QString& imageName) const
{
    static constexpr QChar kSeparator = '.';
    const int separatorPosition = iconName.lastIndexOf(kSeparator);
    imageName = iconName.mid(separatorPosition + 1);
    libraryName = iconName.mid(0, qMax(separatorPosition, 0));
}

QString IconManager::Private::getExtension(const QString& /*libraryName*/) const
{
    static const QString kDefaultExtension = ".svg";
    return kDefaultExtension;
}

QString IconManager::Private::analyticsIconsRoot()
{
    static const QString kAnalyticsIconsRoot = "analytics_icons/";
    return kAnalyticsIconsRoot;
}

} // namespace analytics
} // namespace nx::vms::client::core
