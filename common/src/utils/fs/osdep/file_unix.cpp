/***********************************************************************
*	File: file.cpp
*	Author: Andrey Kolesnikov
*	Date: 13 oct 2006
***********************************************************************/

#if defined(Q_OS_DARWIN) || defined(Q_OS_LINUX)

#include <QDebug>
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
	unsigned int oflag, 
	int* const unixOflag )
{
	int sysFlags = 0;
	if( oflag & QnFile::ofRead )
		sysFlags = O_RDONLY;
	if( oflag & QnFile::ofWrite )
	{
		if( oflag & QnFile::ofRead)
			sysFlags = O_RDWR | O_TRUNC;
		else
			sysFlags = O_WRONLY | O_TRUNC;
		if( !(oflag & QnFile::ofOpenExisting) )
			sysFlags |= O_CREAT;
		if( oflag & QnFile::ofNoTruncate)
		{
			sysFlags &= ~O_TRUNC;
		}
		if( oflag & QnFile::ofAppend )
		{
			sysFlags |= O_APPEND;
			sysFlags &= ~O_TRUNC;
		}

	}
	if( oflag & QnFile::ofCreateNew )
		sysFlags |= O_CREAT | O_EXCL;
	*unixOflag = sysFlags;
}

QnFile::~QnFile()
{
	if( isOpen() )
		close();
}

bool open(QIODevice::OpenMode& mode, unsigned int systemDependentFlags)
{
	if( isOpen() )
		close();

	int sysFlags = 0;
	makeUnixOpenFlags( oflag, &sysFlags );
	m_impl = (void*)::open( fName.toUtf8(), sysFlags | O_LARGEFILE | systemDependentFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );

	return (long)m_impl != -1;
}

bool QnFile::close()
{
	if( ::close( (long)m_impl ) == 0 )
	{
		m_impl = (void*)0xffffffff;
		return true;
	}

	return false;
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
	return m_impl != (void*)0xffffffff;
}

qint64 QnFile::size() const
{
    bool res = false;

    struct stat64 buf;

    if( isOpen() &&  ( fstat64( (long)m_impl, &buf ) == 0 ) )
    {
    	return buf.st_size;
    }
	return -1;
}

qint64 QnFile::seek( qint64 offset)
{
	if( !isOpen() )
		return (qint64)-1;


#ifdef Q_OS_DARWIN
	return lseek( (long)m_impl, offset, SEEK_SET);
#else
	return lseek64( (long)m_impl, offset, SEEK_SET);
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

#endif
