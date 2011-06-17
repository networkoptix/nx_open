#include "filetypesupport.h"
#include "filetypes.h"
#include "util.h"

FileTypeSupport::FileTypeSupport()
{
    for(int i = 0; i < arraysize(IMAGE_FILETYPES); i++)
    {
        m_imageFileExtensions << QString(".") + IMAGE_FILETYPES[i]; 
        m_imageFileFilter << QString("*.") + IMAGE_FILETYPES[i];
    }

    for(int i = 0; i < arraysize(VIDEO_FILETYPES); i++)
    {
        m_movieFileExtensions << QString(".") + VIDEO_FILETYPES[i];
        m_movieFileFilter << QString("*.") + VIDEO_FILETYPES[i];
    }
}

bool FileTypeSupport::isFileSupported(const QString& filename) const
{
    QString lowerFilename = filename.toLower();

    foreach(QString ext, m_imageFileExtensions)
    {
        if (lowerFilename.endsWith(ext))
            return true;
    }

    foreach(QString ext, m_movieFileExtensions)
    {
        if (lowerFilename.endsWith(ext))
            return true;
    }

    return false;
}

const QStringList& FileTypeSupport::imagesFilter() const
{
    return m_imageFileFilter;
}

const QStringList& FileTypeSupport::moviesFilter() const
{
    return m_movieFileFilter;
}
