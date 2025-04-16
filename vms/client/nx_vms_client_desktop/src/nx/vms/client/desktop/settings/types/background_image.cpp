// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "background_image.h"

#include <QtCore/QFileInfo>

#include <nx/reflect/compare.h>

namespace nx::vms::client::desktop {
static const QString kDefaultBackgroundPath = ":/skin/background.png";
static const bool kDefaultBackgroundExists = QFileInfo(kDefaultBackgroundPath).exists();

QString BackgroundImage::imagePath() const
{
    if (!enabled || name.isEmpty())
        return kDefaultBackgroundExists ? kDefaultBackgroundPath : QString();

    return name;
}

bool BackgroundImage::isExternalImageEnabled() const
{
    return enabled && !name.isEmpty() && !name.startsWith(":");
}

bool BackgroundImage::operator==(const BackgroundImage& other) const
{
    return nx::reflect::equals(*this, other);
}

} // namespace nx::vms::client::desktop
