// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::client::core {

class FontLoader
{
public:
    static void loadFonts(const QString& path = QStringLiteral("fonts"));
};

} // namespace nx::vms::client::core
