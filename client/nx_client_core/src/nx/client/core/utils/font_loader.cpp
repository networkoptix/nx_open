#include "font_loader.h"

#include <QtCore/QDir>
#include <QtGui/QFontDatabase>

#include <nx/utils/log/log.h>

namespace {

static nx::utils::log::Tag kTag(lit("nx::client::core::FontLoader"));

} // namespace

namespace nx {
namespace client {
namespace core {

void FontLoader::loadFonts(const QString& path)
{
    const QDir fontsDir(path);

    for (const QString& entry: fontsDir.entryList({QStringLiteral("*.ttf")}, QDir::Files))
    {
        const auto fontFile = fontsDir.absoluteFilePath(entry);
        if (QFontDatabase::addApplicationFont(fontFile) == -1)
            NX_WARNING(kTag, lm("Could not load font %1").arg(fontFile));
        else
            NX_DEBUG(kTag, lm("Font loaded: %1").arg(fontFile));
    }
}

} // namespace core
} // namespace client
} // namespace nx
