#include "filetypesupport.h"

#include "filetypes.h"
#include "utils/common/util.h"

class FileTypeSupportPrivate
{
public:
    FileTypeSupportPrivate();

    QStringList imageFileExtensions;
    QStringList movieFileExtensions;

    QStringList imageFileFilter;
    QStringList movieFileFilter;
};

FileTypeSupportPrivate::FileTypeSupportPrivate()
{
    for (unsigned i = 0; i < arraysize(IMAGE_FILETYPES); i++)
    {
        // ### TODO
        // imageFileExtensions << QLatin1String(".") + QString::fromLatin1(IMAGE_FILETYPES[i]);
        // imageFileFilter << QLatin1String("*.") + QString::fromLatin1(IMAGE_FILETYPES[i]);
        movieFileExtensions << QLatin1String(".") + QString::fromLatin1(IMAGE_FILETYPES[i]);
        movieFileFilter << QLatin1String("*.") + QString::fromLatin1(IMAGE_FILETYPES[i]);
    }

    for (unsigned i = 0; i < arraysize(VIDEO_FILETYPES); i++)
    {
        movieFileExtensions << QLatin1String(".") + QString::fromLatin1(VIDEO_FILETYPES[i]);
        movieFileFilter << QLatin1String("*.") + QString::fromLatin1(VIDEO_FILETYPES[i]);
    }
}

Q_GLOBAL_STATIC(FileTypeSupportPrivate, priv)

bool FileTypeSupport::isMovieFileExt(const QString &filename)
{
    const QString lowerFilename = filename.toLower();
    foreach (const QString &ext, priv()->movieFileExtensions) {
        if (lowerFilename.endsWith(ext))
            return true;
    }

    return false;
}

bool FileTypeSupport::isImageFileExt(const QString &filename)
{
    const QString lowerFilename = filename.toLower();
    foreach (const QString &ext, priv()->imageFileExtensions) {
        if (lowerFilename.endsWith(ext))
            return true;
    }

    return false;
}

bool FileTypeSupport::isFileSupported(const QString &filename)
{
    return isImageFileExt(filename) || isMovieFileExt(filename);
}

const QStringList &FileTypeSupport::imagesFilter()
{
    return priv()->imageFileFilter;
}

const QStringList &FileTypeSupport::moviesFilter()
{
    return priv()->movieFileFilter;
}
