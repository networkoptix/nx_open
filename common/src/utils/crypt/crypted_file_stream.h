#pragma once

#include <QtCore/QString>
#include <QtCore/QFile>

#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {

/**
 * Class that represents a crypted stream in a file.
 * The stream may constitute the whole file or part of it.
 * Additionally provides thread-safe access.
 */

class CryptedFileStream : public QIODevice
{
public:
    struct Stream;

    CryptedFileStream(const QString& fileName);

    virtual ~CryptedFileStream();

    void setEnclosure(qint64 position, qint64 size);

    virtual bool seek(qint64 offset) override;
    virtual qint64 pos() const override;
    virtual qint64 size() const override;

    virtual qint64 readData(char* data, qint64 maxSize) override;
    virtual qint64 writeData(const char* data, qint64 maxSize) override;

    virtual bool open(QIODevice::OpenMode openMode) override;
    virtual void close() override;

    // Equivalent to QnLayoutFileStorageResource::Stream, but there are no common headers.
    struct Stream
    {
        qint64 position = 0;
        qint64 size = 0;
        qint64 originalSize = 0; //< This one is only to make open() reentrant on error.

        bool isNull() { return (position == 0) && (size == 0);}
    } m_enclosure;

protected:
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
    bool readHeader();
    void writeHeader();

    void cryptBlock();
    void decryptBlock();
};

} // namespace utils
} // namespace nx