#ifndef __BUFFERED_FILE_H__
#define __BUFFERED_FILE_H__

#include <memory>
#include <list>
#include <cstdint>

#include <nx/utils/thread/wait_condition.h>
#include <QtCore/QString>
#include <QtCore/QQueue>
#include "utils/fs/file.h"
#include "utils/common/threadqueue.h"
#include "nx/utils/thread/long_runnable.h"
#include "utils/common/byte_array.h"
#include "utils/memory/cycle_buffer.h"
#include <utils/common/connective.h>

class QBufferedFile;

class QN_EXPORT QueueFileWriter: public QnLongRunnable
{
public:
    QueueFileWriter();
    virtual ~QueueFileWriter();

    // write all data in a row
    qint64 writeRanges (QBufferedFile* file, std::vector<QnMediaCyclicBuffer::Range> range);

    /*
    * Returns storage usage in range [0..1]
    */
    float getAvarageUsage();
    virtual void pleaseStop() override;
protected:
    virtual void run();
private:
    void removeOldWritingStatistics(qint64 currentTime);
private:
    struct FileBlockInfo
    {
        FileBlockInfo(QBufferedFile* _file): file(_file) , result(0) {}
        int dataSize() const {
            int result = 0;
            for (const auto& range: ranges)
                result += range.size;
            return result;
        }


        QBufferedFile* file;
        std::vector<QnMediaCyclicBuffer::Range> ranges;
        QnMutex mutex;
        QnWaitCondition condition;
        qint64 result;
    };

    std::list<FileBlockInfo*> m_dataQueue;
    QnMutex m_dataMutex;
    QnWaitCondition m_dataWaitCond;

    typedef QPair<qint64, int> WriteTimingInfo;
    QQueue<WriteTimingInfo> m_writeTimings;
    int m_writeTime;
    mutable QnMutex m_timingsMutex;
private:
    bool putData(FileBlockInfo* fb);
    FileBlockInfo* popData();
};

class QN_EXPORT QBufferedFile: public QIODevice
{
    Q_OBJECT
public:
    /*
    * Buffered file writing.
    * @param ioBlockSize - IO block size
    * @param minBufferSize - do not empty buffer(after IO operation) less then minBufferSize
    */
    QBufferedFile(
        const std::shared_ptr<IQnFile>& fileImpl,
        int ioBlockSize,
        int minBufferSize,
        int maxBufferSize,
        const QnUuid& writerPoolId);
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
    virtual bool atEnd() const override;

    virtual qint64 writeData (const char * data, qint64 len ) override;
    virtual qint64 readData (char * data, qint64 len ) override;

signals:
    void seekDetected(uintptr_t obj, int bufferSizePow);
    void fileClosed(uintptr_t obj);

protected:
    qint64 writeUnbuffered(const char * data, qint64 len );
private:
    bool isWritable() const;
    void flushBuffer();
    bool prepareBuffer(int bufOffset);
    bool updatePos();
    void mergeBufferWithExistingData();
    int writeBuffer(int toWrite);
private:
    std::shared_ptr<IQnFile> m_fileEngine;
    int m_minBufferSize;
    int m_maxBufferSize;
    QnMediaCyclicBuffer m_cycleBuffer;
    QueueFileWriter* m_queueWriter;
    unsigned int m_systemDependentFlags;
private:
    bool m_isDirectIO;
    int m_bufferPos;
    qint64 m_actualFileSize;
    qint64 m_filePos;
    QIODevice::OpenMode m_openMode;

    friend class QueueFileWriter;
    QnByteArray m_cachedBuffer; // cached file begin
    QnByteArray m_tmpBuffer;
    qint64 m_lastSeekPos;
    QnUuid m_writerPoolId;
};

#endif // __BUFFERED_FILE_H__
