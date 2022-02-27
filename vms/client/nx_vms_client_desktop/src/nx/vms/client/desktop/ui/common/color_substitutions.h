// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtGui/QColor>

namespace nx::vms::client::desktop {

using ColorSubstitutions = QMap<QColor, QColor>;

} // namespace nx::vms::client::desktop

inline bool operator<(const QColor& color1, const QColor& color2)
{
    return color1.rgb() < color2.rgb();
}
