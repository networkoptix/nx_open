#include "font_loader.h"

#include <QtCore/QDir>
#include <QtGui/QFontDatabase>

#include <nx/utils/log/log.h>

void QnFontLoader::loadFonts(const QString& path)
{
    QDir fontsDir(path);
    QStringList fontFiles = { QLatin1String("*.ttf") };
    for (const QString& entry: fontsDir.entryList(fontFiles, QDir::Files))
    {
        QString fontFile = fontsDir.absoluteFilePath(entry);
        int id = QFontDatabase::addApplicationFont(fontFile);
        if (id == -1)
        {
            NX_LOG(lit("Could not load font %1").arg(fontFile), cl_logWARNING);
        }
        else
        {
            NX_LOG(lit("Font loaded: %1").arg(fontFile), cl_logDEBUG1);
        }
    }
}
