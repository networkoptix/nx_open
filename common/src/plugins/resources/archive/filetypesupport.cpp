#include "filetypesupport.h"
#include "filetypes.h"
#include "utils/common/util.h"


FileTypeSupport::FileTypeSupport()
{
    for (unsigned i = 0; i < arraysize(IMAGE_FILETYPES); i++)
    {
        // TODO
        // m_imageFileExtensions << QLatin1String(".") + QString::fromLatin1(IMAGE_FILETYPES[i]);
        // m_imageFileFilter << QLatin1String("*.") + QString::fromLatin1(IMAGE_FILETYPES[i]);
        m_movieFileExtensions << QLatin1String(".") + QString::fromLatin1(IMAGE_FILETYPES[i]);
        m_movieFileFilter << QLatin1String("*.") + QString::fromLatin1(IMAGE_FILETYPES[i]);
    }

    for (unsigned i = 0; i < arraysize(VIDEO_FILETYPES); i++)
    {
        m_movieFileExtensions << QLatin1String(".") + QString::fromLatin1(VIDEO_FILETYPES[i]);
        m_movieFileFilter << QLatin1String("*.") + QString::fromLatin1(VIDEO_FILETYPES[i]);
    }
}

bool FileTypeSupport::isMovieFileExt(const QString& filename) const
{
    QString lowerFilename = filename.toLower();

    foreach(QString ext, m_movieFileExtensions)
    {
        if (lowerFilename.endsWith(ext))
            return true;
    }

    return false;
}

bool FileTypeSupport::isImageFileExt(const QString& filename) const
{
    QString lowerFilename = filename.toLower();

    foreach(QString ext, m_imageFileExtensions)
    {
        if (lowerFilename.endsWith(ext))
            return true;
    }

    return false;
}

bool FileTypeSupport::isFileSupported(const QString& filename) const
{
    return isImageFileExt(filename) || isMovieFileExt(filename);
}

const QStringList& FileTypeSupport::imagesFilter() const
{
    return m_imageFileFilter;
}

const QStringList& FileTypeSupport::moviesFilter() const
{
    return m_movieFileFilter;
}
