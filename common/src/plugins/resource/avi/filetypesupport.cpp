#include "filetypesupport.h"

#ifdef ENABLE_ARCHIVE

#include <QtCore/QFile>
extern "C"
{
    #include <libavutil/avutil.h>
}

#include "filetypes.h"
#include "utils/common/util.h"

bool FileTypeSupport::isMovieFileExt(const QString &filename)
{
    const QString lowerFilename = filename.toLower();

    for (unsigned i = 0; i < arraysize(VIDEO_FILETYPES); i++)
    {
        if (lowerFilename.endsWith(QString::fromLatin1(VIDEO_FILETYPES[i])))
            return true;

    }
    return false;
}

bool FileTypeSupport::isImageFileExt(const QString &filename)
{
    const QString lowerFilename = filename.toLower();

    for (unsigned i = 0; i < arraysize(IMAGE_FILETYPES); i++)
    {
        if (lowerFilename.endsWith(QString::fromLatin1(IMAGE_FILETYPES[i])))
            return true;

    }
    return false;
}

bool FileTypeSupport::isLayoutFileExt(const QString &filename)
{
    if (filename.toLower().endsWith(QLatin1String(".nov")))
        return true;
    if (filename.toLower().endsWith(QLatin1String(".exe")))
    {
        QFile f(filename);
        if (f.open(QIODevice::ReadOnly))
        {
            //qint64 pos = f.size() - sizeof(qint64)*3;
            f.seek(f.size() - sizeof(qint64));
            quint64 magic;
            f.read((char*) &magic, sizeof(qint64));
            if (magic == NOV_EXE_MAGIC)
                return true;
        }
    }
    return false;
}

bool FileTypeSupport::isFileSupported(const QString &filename)
{
    return isImageFileExt(filename) || isMovieFileExt(filename) || isLayoutFileExt(filename);
}

#endif // ENABLE_ARCHIVE
