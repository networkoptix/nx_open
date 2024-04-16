// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "skin_image_provider.h"

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

SkinImageProvider::SkinImageProvider(): QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap SkinImageProvider::requestPixmap(const QString& id, QSize* size, const QSize& requestedSize)
{
    if (!NX_ASSERT(!id.isEmpty()))
        return {};

    const auto result = qnSkin->colorizedPixmap(id, requestedSize);
    if (size)
        *size = result.size();

    return result;
}

} // namespace nx::vms::client::core
