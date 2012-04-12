/***********************************************************************
*	File: file.h
*	Author: Andrey Kolesnikov
*	Date: 13 oct 2006
***********************************************************************/

#ifndef _FS_FILE_H_
#define _FS_FILE_H_

#include <QIODevice>
#include <QString>
#include <fcntl.h>

#ifdef WIN32
#pragma warning( disable : 4290 )
#endif


class QN_EXPORT QnFile
{
public:
    QnFile();
    QnFile(const QString& fName);
    virtual ~QnFile();
    void setFileName(const QString& fName) { m_fileName = fName; }
    QString getFileName() const { return m_fileName; }

	virtual bool open(QIODevice::OpenMode& mode, unsigned int systemDependentFlags = 0);
	virtual void close();
	virtual qint64 read(char* buffer, qint64 count);


	virtual qint64 write(const char* buffer, qint64 count);
	virtual void sync();
	virtual bool isOpen() const;
	virtual qint64 size() const;
	virtual bool seek( qint64 offset);
	virtual bool truncate( qint64 newFileSize);
protected:
    QString m_fileName;
private:
	void* m_impl;
};

#endif	//_FS_FILE_H_
