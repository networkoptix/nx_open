// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "svg_icon_provider.h"

#include "skin.h"

namespace nx::vms::client::desktop {

SvgIconProvider::SvgIconProvider(): QQuickImageProvider(QQuickImageProvider::Pixmap)
{}

QPixmap SvgIconProvider::requestPixmap(const QString& id, QSize* size, const QSize& requestedSize)
{
    // Treat null QSize as invalid in order to load the pixmap with original SVG size.
    // Otherwise qnSkin->pixmap() returns an empty image if requested size is null.
    const QSize desiredSize = requestedSize.isNull() ? QSize() : requestedSize;

    // SVG is an intrinsically scalable image, its size does not depend on pixel ratio.
    const auto pixmap = qnSkin->pixmap(":/" + id, /*correctDevicePixelRatio*/ false, desiredSize);
    *size = pixmap.size();
    return pixmap;
}

} // namespace nx::vms::client::desktop
