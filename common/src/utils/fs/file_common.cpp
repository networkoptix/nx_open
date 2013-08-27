/**********************************************************
* 24 jul 2013
* a.kolesnikov
***********************************************************/

#include "file.h"


static void splitPath( const QString& path, QString* dirPath, QString* fileName )
{
#ifdef _WIN32
    //on win32, '/' and '\' is allowed as path separator
    const int slashPos = path.lastIndexOf(QLatin1Char('/'));
    const int reverseSlashPos = path.lastIndexOf(QLatin1Char('\\'));
    const int separatorPos = 
        slashPos == -1
        ? reverseSlashPos 
        : (reverseSlashPos == -1 ? slashPos : std::max<int>(reverseSlashPos, slashPos));
#else
    const int separatorPos = path.lastIndexOf(QLatin1Char('/'));
#endif

    if( separatorPos == -1 )
    {
        if( dirPath )  *dirPath = QString();
        if( fileName ) *fileName = path;
    }
    else
    {
        if( dirPath )  *dirPath = path.mid(0,separatorPos+1);  //including separator with path
        if( fileName ) *fileName = path.mid(separatorPos+1);
    }
}

//!If \a path /etc/smb.conf, returns /etc/, if /etc/ returns /etc/
QString QnFile::filePath( const QString& path )
{
    QString filePath;
    splitPath( path, &filePath, NULL );
    return filePath;
}

//!If \a path /etc/smb.conf, returns smb.conf, for /etc/ returns empty string
QString QnFile::fileName( const QString& path )
{
    QString _fileName;
    splitPath( path, NULL, &_fileName );
    return _fileName;
}

//!If \a path /etc/smb.conf, returns smb
QString QnFile::baseName( const QString& path )
{
    const QString _fileName = fileName( path );
    const int sepPos = _fileName.indexOf(QLatin1Char('.'));
    return sepPos == -1 ? _fileName : _fileName.mid(0, sepPos);
}

QString QnFile::absolutePath( const QString& path )
{
    return QFileInfo(path).absolutePath();
}
