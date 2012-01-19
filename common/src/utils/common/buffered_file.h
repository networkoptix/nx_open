#ifndef __BUFFERED_FILE_H__
#define __BUFFERED_FILE_H__

#include <QString>
#include "utils/fs/file.h"

class QBufferedFile: public QnFile
{
public:
    QBufferedFile(const QString& fileName, int bufferSize = 1024*1024);
    virtual ~QBufferedFile();

    virtual bool open(QIODevice::OpenMode& mode, unsigned int systemDependentFlags);
    virtual qint64	size () const;
    virtual qint64	pos() const;
    virtual void close();
    virtual bool seek(qint64 pos);

    virtual qint64 write (const char * data, qint64 len );
    virtual qint64 read (char * data, qint64 len );
private:
    bool isWritable() const;
    void flushBuffer();
    void disableDirectIO();
private:
    int m_bufferSize;
    quint8* m_buffer;
public:
    bool m_isDirectIO;
    int m_bufferLen;
    int m_bufferPos;
    qint64 m_totalWrited;
    qint64 m_filePos;
    QIODevice::OpenMode m_openMode;
};

#endif // __BUFFERED_FILE_H__
