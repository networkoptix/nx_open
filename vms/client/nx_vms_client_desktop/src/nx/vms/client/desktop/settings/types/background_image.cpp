// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "background_image.h"

#include <QtCore/QFileInfo>

#include <nx/reflect/compare.h>

namespace nx::vms::client::desktop {

qreal BackgroundImage::actualImageOpacity() const
{
    return enabled ? opacity : 0.0;
}

BackgroundImage BackgroundImage::defaultBackground()
{
    static const QString kDefaultBackgroundPath = ":/skin/background.png";
    if (!QFileInfo::exists(kDefaultBackgroundPath))
        return {};

    BackgroundImage result{
        .enabled = true,
        .name = kDefaultBackgroundPath,
    };
    return result;
}

bool BackgroundImage::operator==(const BackgroundImage& other) const
{
    return nx::reflect::equals(*this, other);
}

} // namespace nx::vms::client::desktop
