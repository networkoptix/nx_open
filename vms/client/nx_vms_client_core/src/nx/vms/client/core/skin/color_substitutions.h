// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtGui/QColor>

namespace nx::vms::client::core {

using ColorSubstitutions = QMap<QColor, QColor>;

} // namespace nx::vms::client::core

inline bool operator<(const QColor& color1, const QColor& color2)
{
    return (quint64) color1.rgba64() < (quint64) color2.rgba64();
}
