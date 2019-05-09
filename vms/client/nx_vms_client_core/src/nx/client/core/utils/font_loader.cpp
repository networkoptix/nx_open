#include "font_loader.h"

#include <QtCore/QDir>
#include <QtGui/QFontDatabase>

#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

void FontLoader::loadFonts(const QString& path)
{
    const QDir fontsDir(path);

    for (const QString& entry: fontsDir.entryList({QStringLiteral("*.ttf")}, QDir::Files))
    {
        const auto fontFile = fontsDir.absoluteFilePath(entry);
        if (QFontDatabase::addApplicationFont(fontFile) == -1)
            NX_WARNING(NX_SCOPE_TAG, lm("Could not load font %1").arg(fontFile));
        else
            NX_DEBUG(NX_SCOPE_TAG, lm("Font loaded: %1").arg(fontFile));
    }
}

} // namespace nx::vms::client::core
