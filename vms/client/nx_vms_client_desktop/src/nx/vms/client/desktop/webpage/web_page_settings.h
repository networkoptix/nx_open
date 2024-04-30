// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QSize>
#include <QtCore/QString>

#include <nx/reflect/instrument.h>

namespace nx::vms::client::desktop {

struct WebPageSettings
{
    std::optional<QString> icon;
    std::optional<QSize> windowSize;

    bool operator==(const WebPageSettings&) const = default;
};

NX_REFLECTION_INSTRUMENT(WebPageSettings, (icon)(windowSize))

} // namespace nx::vms::client::desktop
