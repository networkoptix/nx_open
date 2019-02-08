#pragma once

#include <QtCore/QString>

namespace nx::vms::client::core {

class FontLoader
{
public:
    static void loadFonts(const QString& path = QStringLiteral("fonts"));
};

} // namespace nx::vms::client::core
