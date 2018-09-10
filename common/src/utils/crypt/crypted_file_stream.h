#pragma once

#include <vector>

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
    class Key;

    CryptedFileStream(const QString& fileName, const QString& password);

    virtual ~CryptedFileStream();

    void setEnclosure(qint64 position, qint64 size);

    virtual bool open(QIODevice::OpenMode openMode) override;
    virtual void close() override;

    virtual bool seek(qint64 offset) override;
    virtual qint64 pos() const override;
    virtual qint64 size() const override;
    virtual qint64 grossSize() const;

    constexpr static size_t kKeySize = 32; //< Key size in bytes.

    class Key : public std::vector<unsigned char>
    {
    public:
        Key() { resize(kKeySize); }
        Key(std::initializer_list<unsigned char> l): std::vector<unsigned char>(l) { NX_ASSERT(l.size() == kKeySize); }
        explicit Key(const unsigned char * data): std::vector<unsigned char>(data, data + kKeySize) {};
    };

protected:
    constexpr static int kCryptoStreamVersion = 1;
    constexpr static int kCryptoBlockSize = 1024;
    constexpr static int kHeaderSize = 1024;

    // Equivalent to QnLayoutFileStorageResource::Stream, but there are no common headers.
    struct Stream
    {
        qint64 position = 0;
        qint64 size = 0;
        qint64 originalSize = 0; //< This one is only to make open() reentrant on error.

        bool isNull() { return (position == 0) && (size == 0);}
    } m_enclosure;

#pragma pack(push, 4)
    struct Header
    {
        qint64 version = kCryptoStreamVersion;
        qint64 minReadVersion = kCryptoStreamVersion; //< Minimal reader version to access the stream.
        qint64 dataSize = 0;
        unsigned char salt[kKeySize];
        unsigned char keyHash[kKeySize];
    } m_header;
#pragma pack(pop)

    QString m_fileName;
    Key m_passwordKey; //< Hash of adapted password
    Key m_key;

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

    // These functions do all in/out work for the implemented QIODevice interface.
    virtual qint64 readData(char* data, qint64 maxSize) override;
    virtual qint64 writeData(const char* data, qint64 maxSize) override;

    // Helpers.
    void resetState();
    bool isWriting() const { return (m_openMode & WriteOnly); }

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