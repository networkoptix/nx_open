#include "filetypesupport.h"

FileTypeSupport::FileTypeSupport()
{
    m_imageFileExtensions << ".jpg";
    m_imageFileExtensions << ".jpeg";

    m_movieFileExtensions << ".avi";
    m_movieFileExtensions << ".mp4";
    m_movieFileExtensions << ".mkv";
    m_movieFileExtensions << ".wmv";
    m_movieFileExtensions << ".mov";
    m_movieFileExtensions << ".vob";
    m_movieFileExtensions << ".m4v";
    m_movieFileExtensions << ".3gp";
    m_movieFileExtensions << ".mpeg";
    m_movieFileExtensions << ".mpg";
    m_movieFileExtensions << ".mpe";
    m_movieFileExtensions << ".m2ts";
    m_movieFileExtensions << ".flv";

    foreach(QString ext, m_imageFileExtensions)
    {
        m_imageFileFilter << QString("*") + ext;
    }

    foreach(QString ext, m_movieFileExtensions)
    {
        m_movieFileFilter << QString("*") + ext;
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
