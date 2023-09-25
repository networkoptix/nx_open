// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "svg_loader.h"

#include <QtCore/QFile>
#include <QtCore/QVariant>
#include <QtGui/QPainter>
#include <QtGui/QPixmapCache>
#include <QtSvg/QSvgRenderer>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/vms/client/core/skin/color_theme.h>

#include "svg_customization.h"

namespace nx::vms::client::core {

namespace {

QPixmap loadSvgImageUncached(const QString& sourcePath,
    const QMap<QString /*source class name*/, QColor /*target color*/>& customization,
    const QSize& desiredPhysicalSize,
    const qreal devicePixelRatio)
{
    QFile source(sourcePath);
    if (!NX_ASSERT(source.open(QIODevice::ReadOnly), "Cannot load svg file \"%1\"", sourcePath))
        return {};

    const QByteArray sourceData = source.readAll();

    // TODO: VMS-41293 #vkutin #pprivalov #gdm
    // Deprecated substitution of branding colors. Should be switched to color classes mechanism.
    const QByteArray brandedData = substituteSvgColors(sourceData,
        colorTheme()->getColorSubstitutions());

    const QByteArray customizedData = customizeSvgColorClasses(brandedData, customization,
        /*svgName for debug output*/ sourcePath);

    QSvgRenderer renderer;
    if (!NX_ASSERT(renderer.load(customizedData),
        "Error while loading svg image \"%1\"", sourcePath))
    {
        return {};
    }

    const auto physicalSize = desiredPhysicalSize.isEmpty()
        ? renderer.defaultSize() * devicePixelRatio
        : desiredPhysicalSize;

    if (!NX_ASSERT(!physicalSize.isEmpty(),
        "Neither explicit nor default size is available for svg image \"%1\"", sourcePath))
    {
        return {};
    }

    QPixmap result(physicalSize);
    result.fill(Qt::transparent);

    QPainter p;
    NX_ASSERT(p.begin(&result));
    renderer.render(&p);
    NX_ASSERT(p.end());

    result.setDevicePixelRatio(devicePixelRatio);
    return result;
}

} // namespace

QPixmap loadSvgImage(const QString& sourcePath,
    const QMap<QString /*source class name*/, QColor /*target color*/>& customization,
    const QSize& desiredPhysicalSize,
    const qreal devicePixelRatio)
{
    QStringList customizationKey;
    for (const auto& [className, color]: customization.asKeyValueRange())
        customizationKey.push_back(nx::format("%1=%2", className, color.name(QColor::HexArgb)));

    const QString cacheKey = nx::format("%1\n%2\n%3\n%4",
        sourcePath, customizationKey.join("&"), desiredPhysicalSize, devicePixelRatio);

    QPixmap pixmap;
    if (QPixmapCache::find(cacheKey, &pixmap))
        return pixmap;

    pixmap = loadSvgImageUncached(sourcePath, customization, desiredPhysicalSize, devicePixelRatio);
    QPixmapCache::insert(cacheKey, pixmap);
    return pixmap;
}

QPixmap loadSvgImage(const QString& sourcePath,
    const QMap<QString /*source class name*/, QString /*target color name*/>& customization,
    const QSize& desiredPhysicalSize,
    const qreal devicePixelRatio)
{
    QMap<QString /*source class name*/, QColor /*target color*/> effectiveCustomization;

    for (const auto& [sourceClassName, targetColorName]: customization.asKeyValueRange())
    {
        const auto targetColor = colorTheme()->colors().contains(targetColorName)
            ? colorTheme()->colors().value(targetColorName).value<QColor>()
            : QColor::fromString(targetColorName);

        NX_ASSERT(targetColor.isValid(),
            "Invalid color \"%1\" in customization for svg image \"%2\"",
            targetColorName, sourcePath);

        effectiveCustomization.insert(sourceClassName, targetColor);
    }

    return loadSvgImage(sourcePath, effectiveCustomization, desiredPhysicalSize, devicePixelRatio);
}

} // namespace nx::vms::client::core
