/***********************************************************************
*	File: file.cpp
*	Author: Andrey Kolesnikov
*	Date: 13 oct 2006
***********************************************************************/

#include <qglobal.h>

#if defined(Q_OS_DARWIN) || defined(Q_OS_LINUX)

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include "../file.h"

#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>

#ifdef Q_OS_DARWIN
#define O_LARGEFILE 0
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

QnFile::QnFile(): m_impl(0)
{

}

QnFile::QnFile(const QString& fName): m_fileName(fName), m_impl(0)
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

    if (handle == -1)
        m_impl = 0;
    else
        m_impl = (void*) handle;

    return m_impl != 0;
}

void QnFile::close()
{
    if( ::close( (long)m_impl ) == 0 )
	{
        m_impl = 0;
	}
}

qint64 QnFile::read( char* buffer, qint64 count )
{
	if( !isOpen() )
		return -1;
	return ::read( (long)m_impl, buffer, count );
}

qint64 QnFile::write( const char* buffer, qint64 count )
{
	if( !isOpen() )
		return -1;
	return ::write( (long)m_impl, buffer, count );
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


#ifdef Q_OS_DARWIN
    return lseek( (long)m_impl, offset, SEEK_SET) != -1;
#else
    return lseek64( (long)m_impl, offset, SEEK_SET) != -1;
#endif	
}

bool QnFile::truncate( qint64 newFileSize )
{
#ifdef Q_OS_DARWIN
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

#endif
