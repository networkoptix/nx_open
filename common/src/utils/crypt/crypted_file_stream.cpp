#include "crypted_file_stream.h"

#include <algorithm>
#include <vector>
#include <random>

//#include <openssl/evp.h>

using Key = nx::utils::CryptedFileStream::Key;

namespace {

constexpr size_t kKeySize = nx::utils::CryptedFileStream::kKeySize;

const Key kHashSalt{
    0x89, 0x1e, 0xed, 0x37, 0xb9, 0x5f, 0xcc, 0x9f, 0xd0, 0x3b, 0x29, 0x7e, 0x59, 0x6d, 0xed, 0xe,
    0x9c, 0x3a, 0x25, 0x2f, 0xf8, 0xb8, 0xc8, 0x98, 0x1f, 0xa3, 0xbb, 0x31, 0x67, 0x10, 0x7a, 0x52
};

const Key kPasswordSalt{
    0x31, 0xc6, 0x82, 0x69, 0xbc, 0x8d, 0xf7, 0x91, 0x2e, 0xd8, 0x2d, 0xd7, 0xbf, 0x5b, 0x99, 0xe,
    0x83, 0xc6, 0xe9, 0x9e, 0xdf, 0x69, 0x5e, 0x4e, 0x8b, 0xa5, 0xd7, 0xbc, 0x8b, 0xb3, 0xf2, 0x6
};

Key adaptPassword(const char* password)
{
    static char zero = 0;
    if (!password) //< Just in case - we need password[0] to be valid.
        password = &zero;

    const size_t passLength = std::max((size_t) 1, strlen(password));
    Key key = kPasswordSalt;
    for (size_t i = 0; i < std::max(passLength, kKeySize); i++)
        key[i % kKeySize] = password[i % passLength] ^ key[i % kKeySize];
    return key;
}

Key xorKeys(const Key& key1, const Key& key2)
{
    Key key;
    for (int i = 0; i < kKeySize; i++)
        key[i] = key1[i] ^ key2[i];
    return key;
}

Key getKeyHash(const Key& key)
{
    Key xored = xorKeys(key, kHashSalt);
    // SHA goes here;
    return xored;
}

// This function generates salt; need not to be cryptographically strong random.
Key getRandomSalt()
{
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, 255);
    generator.seed(time(nullptr));

    Key key;
    for (int i = 0; i < kKeySize; i++)
        key[i] = distribution(generator);

    return key;
}

} // namespace

namespace nx {
namespace utils {

CryptedFileStream::CryptedFileStream(const QString& fileName, const QString& password):
    m_fileName(fileName),
    m_file(fileName),
    m_mutex(QnMutex::Recursive)
{
    resetState();
    m_passwordKey = adaptPassword(password.toUtf8().constData()); //< Convert to utf8 and adapt to Key.
}

CryptedFileStream::~CryptedFileStream()
{
    close();
}

void CryptedFileStream::setEnclosure(qint64 position, qint64 size)
{
    NX_ASSERT(m_openMode == NotOpen);
    m_enclosure.position = position;
    m_enclosure.size = size;
    m_enclosure.originalSize = size;
}

bool CryptedFileStream::open(QIODevice::OpenMode openMode)
{
    // TODO: Add error-checking.
    QnMutexLocker lock(&m_mutex);
    close();

    m_file.setFileName(m_fileName);

    OpenMode fileOpenMode = openMode && (~OpenMode(Text)); //< Clear Text flag from underlying file.
    // Since we may have to read unfinished blocks, we should never open in WriteOnly mode.
    // For Append we need to read header and modify last block.
    if (openMode == WriteOnly || (openMode & Append))
        fileOpenMode = ReadWrite;

    if (!m_file.open(fileOpenMode))
        return false;

    m_openMode = openMode;

    m_enclosure.size = m_enclosure.originalSize;

    if (m_enclosure.isNull() && openMode != WriteOnly) //< Adjust to file size except if WriteOnly.
        m_enclosure.size = m_file.size();

    if (openMode == WriteOnly)
        createHeader();
    else
    {
        if (!readHeader())
            return false;
    }

    m_openMode = openMode;
    QIODevice::open(openMode);

    if (openMode & Append)
        seek(m_header.dataSize);
    else
        seek(0);

    return true;
}

void CryptedFileStream::close()
{
    QnMutexLocker lock(&m_mutex);

    bool Writing = isWriting();
    if (m_openMode != NotOpen && isWriting())
    {
        dumpCurrentBlock();
        writeHeader();
    }

    m_file.close();
    QIODevice::close();

    resetState();
}

qint64 CryptedFileStream::readData(char* data, qint64 maxSize)
{
    // TODO: Add error-checking & read data counting.
    QnMutexLocker lock(&m_mutex);
    maxSize = std::min(maxSize, m_header.dataSize - m_position.position());
    qint64 toRead = maxSize;

    const qint64 head = std::min(maxSize, kCryptoBlockSize - m_position.positionInBlock);
    readFromBlock(data, head);
    toRead -= head;

    while (toRead > kCryptoBlockSize)
    {
        advanceBlock();
        readFromBlock(data + (maxSize - toRead), kCryptoBlockSize);
        toRead -= kCryptoBlockSize;
    }

    if (toRead > 0)
    {
        advanceBlock();
        readFromBlock(data + (maxSize - toRead), toRead);
    }

    return maxSize;
}

qint64 CryptedFileStream::writeData(const char* data, qint64 maxSize)
{
    // TODO: Add error-checking & write data counting.
    QnMutexLocker lock(&m_mutex);
    qint64 toWrite = maxSize;

    const qint64 head = std::min(maxSize, kCryptoBlockSize - m_position.positionInBlock);
    writeToBlock(data, head);
    toWrite -= head;

    while (toWrite > kCryptoBlockSize)
    {
        advanceBlock();
        writeToBlock(data + (maxSize - toWrite), kCryptoBlockSize);
        toWrite -= kCryptoBlockSize;
    }

    if (toWrite > 0)
    {
        advanceBlock();
        writeToBlock(data + (maxSize - toWrite), toWrite);
    }

    return maxSize;
}

bool CryptedFileStream::seek(qint64 offset)
{
    QnMutexLocker lock(&m_mutex);

    if (!isWriting() && (offset > m_header.dataSize))
        return false;

    dumpCurrentBlock();
    m_position.setPosition(offset);
    loadCurrentBlock();

    QIODevice::seek(offset);
    return true;
}

qint64 CryptedFileStream::pos() const
{
    QnMutexLocker lock(&m_mutex);
    return QIODevice::pos();
}

qint64 CryptedFileStream::size() const
{
    QnMutexLocker lock(&m_mutex);
    return m_header.dataSize;
}

qint64 CryptedFileStream::grossSize() const
{
    // Calculating block number including "tail" block.
    return kHeaderSize
        + ((m_header.dataSize + kCryptoBlockSize - 1) / kCryptoBlockSize) * kCryptoBlockSize;
}

void CryptedFileStream::resetState()
{
    m_blockDirty = false;
    m_position = Position();
    m_header = Header();
    memset(m_currentPlainBlock, 0, kCryptoBlockSize);
    memset(m_header.salt, 0, kKeySize);
    m_openMode = NotOpen;
}

void CryptedFileStream::readFromBlock(char* data, qint64 count)
{
    NX_ASSERT(count + m_position.positionInBlock <= kCryptoBlockSize);
    memcpy(data, m_currentPlainBlock + m_position.positionInBlock, count);
    m_position.positionInBlock += count;
}

void CryptedFileStream::writeToBlock(const char* data, qint64 count)
{
    if (count == 0) //< Nothing to write.
        return;

    NX_ASSERT(count + m_position.positionInBlock <= kCryptoBlockSize);
    memcpy(m_currentPlainBlock + m_position.positionInBlock, data, count);

    m_blockDirty = true;
    m_position.positionInBlock += count;

    m_header.dataSize = std::max(m_header.dataSize, m_position.position());
}

void CryptedFileStream::advanceBlock()
{
    dumpCurrentBlock();
    m_position.blockIndex ++;
    m_position.positionInBlock = 0;
    loadCurrentBlock();
}

void CryptedFileStream::dumpCurrentBlock()
{
    if (!m_blockDirty || !isWriting())
        return;

    cryptBlock();
    m_file.seek(m_enclosure.position + kHeaderSize + kCryptoBlockSize * m_position.blockIndex);
    m_file.write(m_currentCryptedBlock, kCryptoBlockSize);
    m_blockDirty = false;
}

void CryptedFileStream::loadCurrentBlock()
{
    bool shouldRead = isWriting()
        // Will load if there is any data in the block.
        ? ((m_position.blockIndex + 1) * kCryptoBlockSize - m_header.dataSize > 0)
        // Will load if entire block fits into enclosure.
        : ((m_position.blockIndex + 1) * kCryptoBlockSize <= m_enclosure.size);

    if (shouldRead)
    {
        m_file.seek(m_enclosure.position + kHeaderSize + kCryptoBlockSize * m_position.blockIndex);
        m_file.read(m_currentCryptedBlock, kCryptoBlockSize);
        decryptBlock();
    }
}

void CryptedFileStream::createHeader()
{
    m_header = Header();

    Key salt = getRandomSalt();
    std::copy_n(salt.begin(), kKeySize, m_header.salt);
    m_key = xorKeys(m_passwordKey, salt);
    Key keyHash = getKeyHash(m_key);
    std::copy_n(keyHash.begin(), kKeySize, m_header.keyHash);

    writeHeader();
}

bool CryptedFileStream::readHeader()
{
    if (m_enclosure.size < kHeaderSize)
        return false;

    m_file.seek(m_enclosure.position);
    if (m_file.read((char *)&m_header, sizeof(m_header)) != sizeof(m_header))
        return false;

    m_key = xorKeys(m_passwordKey, Key(m_header.salt));

    return getKeyHash(m_key) == Key(m_header.keyHash);
}

void CryptedFileStream::writeHeader()
{
    m_file.seek(m_enclosure.position);
    m_file.write((char *)&m_header, sizeof(m_header));
    // Fill rest with zeroes.
    static char zeroes[kHeaderSize - sizeof(m_header)];
    memset(zeroes, 0, kHeaderSize - sizeof(m_header));
    m_file.write(zeroes, kHeaderSize - sizeof(m_header));
}

void CryptedFileStream::cryptBlock()
{
    memcpy(m_currentCryptedBlock, m_currentPlainBlock,  kCryptoBlockSize);
}

void CryptedFileStream::decryptBlock()
{
    memcpy(m_currentPlainBlock, m_currentCryptedBlock, kCryptoBlockSize);
}

} // namespace utils
} // namespace nx
