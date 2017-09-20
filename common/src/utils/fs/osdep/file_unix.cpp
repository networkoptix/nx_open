/***********************************************************************
*	File: file.cpp
*	Author: Andrey Kolesnikov
*	Date: 13 oct 2006
***********************************************************************/

#include <qglobal.h>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <nx/utils/log/log.h>
#include "../file.h"

#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>

#ifdef Q_OS_DARWIN
#define O_LARGEFILE 0
#endif

#if defined(Q_OS_IOS) || (defined(Q_OS_ANDROID) && __ANDROID_API__ < 21)
#define stat64 stat
#define fstat64 fstat
#endif

void makeUnixOpenFlags( 
    const QIODevice::OpenMode& oflag,
	int* const unixOflag )
{
	int sysFlags = 0;
    if( oflag & QIODevice::ReadOnly )
		sysFlags = O_RDONLY;
    if( oflag & QIODevice::WriteOnly )
	{
        if( oflag & QIODevice::ReadOnly)
			sysFlags = O_RDWR | O_TRUNC;
		else
			sysFlags = O_WRONLY | O_TRUNC;
        if( !(oflag & QIODevice::Append) )
            sysFlags |= O_CREAT;
        if( !(oflag & QIODevice::Truncate))
		{
			sysFlags &= ~O_TRUNC;
		}
        if( oflag & QIODevice::Append )
		{
			sysFlags |= O_APPEND;
			sysFlags &= ~O_TRUNC;
		}

	}
	*unixOflag = sysFlags;
}

QnFile::QnFile(): m_impl(0), m_eof(false)
{

}

QnFile::QnFile(const QString& fName): m_fileName(fName), m_impl(0), m_eof(false)
{

}

QnFile::~QnFile()
{
	if( isOpen() )
		close();
}

bool QnFile::open(const QIODevice::OpenMode& mode, unsigned int systemDependentFlags)
{
	if( isOpen() )
		close();

    int sysFlags = 0;
    makeUnixOpenFlags( mode, &sysFlags );
    int handle = ::open( m_fileName.toUtf8(), sysFlags | O_LARGEFILE | systemDependentFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );

    if (handle == -1 && (mode & QIODevice::WriteOnly) && errno == ENOENT)
    {
        QDir dir;
        if (dir.mkpath(QnFile::absolutePath(m_fileName))) 
            handle = ::open( m_fileName.toUtf8(), sysFlags | O_LARGEFILE | systemDependentFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
    }

    if (handle == -1)
        m_impl = 0;
    else
        m_impl = reinterpret_cast<void*>(handle);

    return m_impl != 0;
}

void QnFile::close()
{
    ::close( (long)m_impl );
    m_impl = 0;
}

bool QnFile::eof() const { return m_eof; }

qint64 QnFile::read( char* buffer, qint64 count )
{
	if( !isOpen() )
		return -1;
    ssize_t bytesRead = ::read((long)m_impl, buffer, count);
    if (count > 0 && bytesRead == 0)
        m_eof = true;
	return bytesRead;
}

qint64 QnFile::write( const char* buffer, qint64 count )
{
    auto printLogMessage = [](const QString& m, QnLogLevel l)
    {
        NX_LOG(lit("QnFile::write failed. %1 Errno: %2")
                .arg(m)
                .arg(errno),
            cl_logERROR);
    };
	if( !isOpen() )
    {
        printLogMessage(lit("File is not opened."), cl_logERROR);
		return -1;
    }
	ssize_t writtenBytes = ::write( (long)m_impl, buffer, count );
    if (writtenBytes == -1)
        printLogMessage(lit(""), cl_logERROR);
    else if (writtenBytes < count) 
        printLogMessage(lit("Failed to write all bytes."), cl_logERROR);

    return writtenBytes;
}

bool QnFile::isOpen() const
{
    return m_impl != 0;
}

qint64 QnFile::size() const
{
    struct stat64 buf;

    if( isOpen() &&  ( fstat64( (long)m_impl, &buf ) == 0 ) )
    {
    	return buf.st_size;
    }
	return -1;
}

bool QnFile::seek( qint64 offset)
{
	if( !isOpen() )
        return false;


#if defined(Q_OS_DARWIN) || defined(Q_OS_ANDROID)
    return lseek( (long)m_impl, offset, SEEK_SET) != -1;
#else
    return lseek64( (long)m_impl, offset, SEEK_SET) != -1;
#endif	
}

bool QnFile::truncate( qint64 newFileSize )
{
#if defined(Q_OS_DARWIN) || defined(Q_OS_ANDROID)
	return ftruncate( (long)m_impl, newFileSize ) == 0;
#else
	return ftruncate64( (long)m_impl, newFileSize ) == 0;
#endif	
}

void QnFile::sync()
{
	::sync();
}

bool QnFile::fileExists( const QString& fileName )
{
    struct stat64 fstat;
    int retCode = stat64( fileName.toLocal8Bit(), &fstat );
    return retCode == 0;
}

qint64 QnFile::getFileSize( const QString& fileName )
{
    struct stat64 fstat;
    int retCode = stat64( fileName.toLocal8Bit(), &fstat );
    if( retCode != 0 )
        return -1;
    return fstat.st_size;
}
