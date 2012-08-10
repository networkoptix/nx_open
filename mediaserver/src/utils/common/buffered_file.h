#ifndef __BUFFERED_FILE_H__
#define __BUFFERED_FILE_H__

#include <QWaitCondition>
#include <QString>
#include <QQueue>
#include "utils/fs/file.h"
#include "utils/common/threadqueue.h"
#include "utils/common/longrunnable.h"

class QBufferedFile;

class QN_EXPORT QueueFileWriter: public QnLongRunnable
{
    Q_OBJECT;

public:
    QueueFileWriter();
    virtual ~QueueFileWriter();

    // write all data in a row
    qint64 write (QBufferedFile* file, const char * data, qint64 len);
    
    /*
    * Returns storage usage in range [0..1]
    */
    float getAvarageUsage();
protected:
    virtual void run();
private:
    void removeOldWritingStatistics(qint64 currentTime);
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

    typedef QPair<qint64, int> WriteTimingInfo;
    QQueue<WriteTimingInfo> m_writeTimings;
    int m_writeTime;
    mutable QMutex m_timingsMutex;
};

class QnWriterPool
{
public:
    typedef QMap<QString, QueueFileWriter*> WritersMap;

    QnWriterPool();
    ~QnWriterPool();

    static QnWriterPool* instance();

    QueueFileWriter* getWriter(const QString& fileName);
    WritersMap getAllWriters();
private:
    QMutex m_mutex;
    WritersMap m_writers;
};

class QN_EXPORT QBufferedFile: public QIODevice
{
public:
    /*
    * Buffered file writing.
    * @param ioBlockSize - IO block size
    * @param minBufferSize - do not empty buffer(after IO operation) less then minBufferSize
    */
    QBufferedFile(const QString& fileName, int ioBlockSize, int minBufferSize);
    virtual ~QBufferedFile();

    /*
    * Addition system depended io flags
    */
    void setSystemFlags(int setSystemFlags);

    virtual bool open(QIODevice::OpenMode mode) override;
    virtual qint64 size() const override;
    virtual qint64 pos() const override;
    virtual void close() override;
    virtual bool seek(qint64 pos) override;

    virtual qint64 writeData (const char * data, qint64 len ) override;
    virtual qint64 readData (char * data, qint64 len ) override;
    
protected:
    qint64 writeUnbuffered(const char * data, qint64 len );
private:
    bool isWritable() const;
    void flushBuffer();
    void disableDirectIO();
private:
    QnFile m_fileEngine;
    int m_bufferSize;
    int m_minBufferSize;
    quint8* m_buffer;
    QueueFileWriter* m_queueWriter;
    unsigned int m_systemDependentFlags;
private:
    bool m_isDirectIO;
    int m_bufferLen;
    int m_bufferPos;
    qint64 m_totalWrited;
    qint64 m_filePos;
    QIODevice::OpenMode m_openMode;

    friend class QueueFileWriter;
};

#endif // __BUFFERED_FILE_H__
