#pragma once

#include <QtCore/QString>

namespace nx {
namespace client {
namespace core {

class FontLoader
{
public:
    static void loadFonts(const QString& path = QStringLiteral("fonts"));
};

} // namespace core
} // namespace client
} // namespace nx
