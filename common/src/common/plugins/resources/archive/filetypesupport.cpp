#include "filetypesupport.h"

#include "filetypes.h"
#include "common/util.h"

FileTypeSupport::FileTypeSupport()
{
    for (int i = 0; i < arraysize(IMAGE_FILETYPES); ++i)
    {
        m_imageFileExtensions << QLatin1Char('.') + QString::fromLatin1(IMAGE_FILETYPES[i]);
        m_imageFileFilter << QLatin1String("*.") + QString::fromLatin1(IMAGE_FILETYPES[i]);
    }

    for (int i = 0; i < arraysize(VIDEO_FILETYPES); ++i)
    {
        m_movieFileExtensions << QLatin1Char('.') + QString::fromLatin1(VIDEO_FILETYPES[i]);
        m_movieFileFilter << QLatin1String("*.") + QString::fromLatin1(VIDEO_FILETYPES[i]);
    }
}

bool FileTypeSupport::isFileSupported(const QString &filename) const
{
    return isImageFileExt(filename) || isMovieFileExt(filename);
}

bool FileTypeSupport::isImageFileExt(const QString &filename) const
{
    QString lowerFilename = filename.toLower();

    foreach (const QString &ext, m_imageFileExtensions)
    {
        if (lowerFilename.endsWith(ext))
            return true;
    }

    return false;
}

bool FileTypeSupport::isMovieFileExt(const QString &filename) const
{
    QString lowerFilename = filename.toLower();

    foreach(const QString &ext, m_movieFileExtensions)
    {
        if (lowerFilename.endsWith(ext))
            return true;
    }

    return false;
}

const QStringList &FileTypeSupport::imagesFilter() const
{
    return m_imageFileFilter;
}

const QStringList &FileTypeSupport::moviesFilter() const
{
    return m_movieFileFilter;
}
