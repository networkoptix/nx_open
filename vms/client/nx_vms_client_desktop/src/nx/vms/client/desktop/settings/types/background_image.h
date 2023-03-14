// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <client/client_globals.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::client::desktop {

struct BackgroundImage
{
    bool enabled = false;
    QString name;
    QString originalName;
    Qn::ImageBehavior mode = Qn::ImageBehavior::Crop;
    qreal opacity = 0.5;

    qreal actualImageOpacity() const;

    static BackgroundImage defaultBackground();

    bool operator==(const BackgroundImage& other) const;
};

NX_REFLECTION_INSTRUMENT(BackgroundImage, (enabled)(name)(originalName)(mode)(opacity))

} // namespace nx::vms::client::desktop
