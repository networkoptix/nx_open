#ifndef __BUFFERED_FILE_H__
#define __BUFFERED_FILE_H__

#include <QString>
#include "utils/fs/file.h"
#include "longrunnable.h"
#include "threadqueue.h"

class QBufferedFile;

class QueueFileWriter: public CLLongRunnable
{
    Q_OBJECT;

public:
    QueueFileWriter();
    virtual ~QueueFileWriter();

    // write all data in a row
    qint64 write (QBufferedFile* file, const char * data, qint64 len);
protected:
    virtual void run();
private:
    struct FileBlockInfo
    {
        FileBlockInfo(): file(0), data(0), len(0), result(0) {}
        FileBlockInfo(QBufferedFile* _file, const char * _data, qint64 _len): file(_file), data(_data), len(_len), result(0) {}
        QBufferedFile* file;
        const char* data;
        int len;
        QMutex mutex;
        QWaitCondition condition;
        qint64 result;
    };

    CLThreadQueue<FileBlockInfo*> m_dataQueue;
};

class QBufferedFile: public QnFile
{
public:
    /*
    * Buffered file writing.
    * @param ioBlockSize - IO block size
    * @param minBufferSize - do not empty buffer(after IO operation) less then minBufferSize
    */
    QBufferedFile(const QString& fileName, int ioBlockSize, int minBufferSize);
    virtual ~QBufferedFile();

    virtual bool open(QIODevice::OpenMode& mode, unsigned int systemDependentFlags);
    virtual qint64	size () const;
    virtual qint64	pos() const;
    virtual void close();
    virtual bool seek(qint64 pos);

    qint64 writeUnbuffered(const char * data, qint64 len );
    virtual qint64 write (const char * data, qint64 len );
    virtual qint64 read (char * data, qint64 len );
private:
    bool isWritable() const;
    void flushBuffer();
    void disableDirectIO();
private:
    int m_bufferSize;
    int m_minBufferSize;
    quint8* m_buffer;
    static QueueFileWriter m_queueWriter;
public:
    bool m_isDirectIO;
    int m_bufferLen;
    int m_bufferPos;
    qint64 m_totalWrited;
    qint64 m_filePos;
    QIODevice::OpenMode m_openMode;
};

#endif // __BUFFERED_FILE_H__
