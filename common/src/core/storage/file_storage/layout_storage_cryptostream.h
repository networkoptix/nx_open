#pragma once

#include <QtCore/QString>
#include <QtCore/QFile>

#include <nx/utils/thread/mutex.h>

#include "layout_storage_stream.h"
#include "layout_storage_resource.h"

class QnLayoutCryptoStream : public QnLayoutStream
{
public:
    QnLayoutCryptoStream(QnLayoutFileStorageResource& storageResource, const QString& fileName);

    virtual ~QnLayoutCryptoStream();

    virtual bool seek(qint64 offset) override;
    virtual qint64 pos() const override;
    virtual qint64 size() const override;

    virtual qint64 readData(char* data, qint64 maxSize) override;
    virtual qint64 writeData(const char* data, qint64 maxSize) override;

    virtual bool open(QIODevice::OpenMode openMode) override;
    virtual void close() override;

    virtual void lockFile() override;
    virtual void unlockFile() override;

    virtual void storeStateAndClose() override;
    virtual void restoreState() override;

private:
    constexpr static int kCryptoStreamVersion = 1;
    constexpr static int kCryptoBlockSize = 1024;
    constexpr static int kHeaderSize = 1024;

#pragma pack(push, 4)
    struct Header
    {
        qint64 version = kCryptoStreamVersion;
        qint64 dataSize = 0;
    } m_header;
#pragma pack(pop)

    QnLayoutFileStorageResource::Stream m_enclosure;

    QnLayoutFileStorageResource& m_storageResource;

    QString m_fileName;

    struct Position
    {
        qint64 blockIndex = 0;
        qint64 positionInBlock  = 0;

        bool dirty = false; //< Data in decrypted block is not flashed.

        void setPosition(qint64 position) { blockIndex = position / kCryptoBlockSize;
            positionInBlock = position % kCryptoBlockSize; }
        qint64 position() const { return blockIndex * kCryptoBlockSize + positionInBlock; }
    } m_position;

    OpenMode m_openMode = NotOpen;

    // These are used for store/restore functionality that is used for NOV files moving.
    qint64 m_storedPosition = 0;
    OpenMode m_storedOpenMode = NotOpen;

    char m_currentPlainBlock[kCryptoBlockSize];
    char m_currentCryptedBlock[kCryptoBlockSize];
    bool m_blockDirty = false; //< Data in decrypted block was not flushed.

    QFile m_file;
    mutable QnMutex m_mutex;

    // Helpers.
    bool isWriting() const { return (m_openMode | WriteOnly) || (m_openMode | Append); }
    void flush();
    void adjustSize();

    // Internal block functions.
    void readFromBlock(char* data, qint64 count);
    void writeToBlock(const char* data, qint64 count);
    void advanceBlock();

    void dumpCurrentBlock();
    void loadCurrentBlock();

    // Working with stream header.
    void createHeader();
    void readHeader();
    void writeHeader();

    void cryptBlock();
    void decryptBlock();
};
