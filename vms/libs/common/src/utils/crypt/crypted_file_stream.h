#pragma once

#include <array>

#include <QtCore/QString>
#include <QtCore/QFile>

#include <utils/crypt/crypto_functions.h>
#include <nx/utils/thread/mutex.h>

namespace nx::utils {

/**
 * Class that represents a crypted stream in a file.
 * The stream may constitute the whole file or part of it.
 * Additionally provides thread-safe access.
 */

class CryptedFileStream: public QIODevice
{
    using Key = crypto_functions::Key;

public:
    CryptedFileStream(const QString& fileName, const QString& password = QString());
    CryptedFileStream(const QString& fileName, qint64 position, qint64 size,
        const QString& password = QString());
    virtual ~CryptedFileStream();

    // Set cryptostream bounds for emplacement in another file-container.
    void setEnclosure(qint64 position, qint64 size);

    void setPassword(const QString& password);

    virtual bool open(QIODevice::OpenMode openMode) override;
    virtual void close() override;

    virtual bool seek(qint64 offset) override;
    virtual qint64 pos() const override;
    virtual qint64 size() const override;
    virtual qint64 grossSize() const;

protected:
    mutable QnMutex m_mutex;
    OpenMode m_openMode = NotOpen;

    bool isWriting() const { return (m_openMode & WriteOnly); }

    // Equivalent to QnLayoutFileStorageResource::Stream, but there are no common headers.
    struct Stream
    {
        qint64 position = 0;
        qint64 size = 0;
        qint64 originalSize = 0; //< This one is only to make open() reentrant on error.

        bool isNull() const { return (position == 0) && (size == 0);}
    } m_enclosure;

    struct Position
    {
        qint64 blockIndex = 0;
        qint64 positionInBlock  = 0;

        bool dirty = false; //< Data in decrypted block is not flashed.

        void setPosition(qint64 position) { blockIndex = position / kCryptoBlockSize;
        positionInBlock = position % kCryptoBlockSize; }
        qint64 position() const { return blockIndex * kCryptoBlockSize + positionInBlock; }
    } m_position;

private:
    constexpr static int kCryptoStreamVersion = 1;
    constexpr static int kCryptoBlockSize = 1024;
    constexpr static int kHeaderSize = 1024;

#pragma pack(push, 4)
    struct Header
    {
        qint64 version = kCryptoStreamVersion;
        qint64 minReadVersion = kCryptoStreamVersion; //< Minimal reader version to access the stream.
        qint64 dataSize = 0;
        Key salt = {};
        Key keyHash = {};
    } m_header;
#pragma pack(pop)

    QString m_fileName;
    Key m_passwordKey = {}; //< Hash of adapted password
    Key m_key = {};

    char m_currentPlainBlock[kCryptoBlockSize];
    char m_currentCryptedBlock[kCryptoBlockSize];
    void* m_context; //< Using void* because EVP_CIPHER_CTX is a OpenSSL typedef.
    void* m_mdContext; //< Using void* because EVP_MD_CTX is a OpenSSL typedef.
    Key m_IV = {};
    bool m_blockDirty = false; //< Data in decrypted block was not flushed.

    QFile m_file;

    // These functions do all in/out work for the implemented QIODevice interface.
    virtual qint64 readData(char* data, qint64 maxSize) override;
    virtual qint64 writeData(const char* data, qint64 maxSize) override;

    // Helpers.
    void resetState();

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

} // namespace nx::utils
