// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "svg_image_provider.h"

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <nx/vms/client/core/skin/svg_loader.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

SvgImageProvider::SvgImageProvider(): QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap SvgImageProvider::requestPixmap(const QString& id, QSize* size, const QSize& requestedSize)
{
    if (!NX_ASSERT(!id.isEmpty()))
        return {};

    const QUrl sourceUrl(id.startsWith(":/") ? ("qrc" + id) : id);
    const QString scheme = sourceUrl.scheme();
    const bool isResource = scheme.isEmpty() || scheme == "qrc";

    if (!NX_ASSERT(isResource || scheme == "file", "Invalid svg image location: \"%1\"", id))
        return {};

    QString sourcePath = sourceUrl.path();
    if (isResource && !sourcePath.startsWith(":/"))
        sourcePath = ":/" + sourcePath;

    QMap<QString /*source class name*/, QString /*target color name*/> customization;
    for (const auto& [className, colorName]: QUrlQuery(sourceUrl.query()).queryItems())
        customization[className] = colorName;

    if (requestedSize.isEmpty())
        NX_INFO(this, "Requested an SVG image with default size: \"%1\"", sourcePath);

    const auto result = loadSvgImage(sourcePath, customization, requestedSize);
    if (size)
        *size = result.size();
    return result;
}

} // namespace nx::vms::client::core
