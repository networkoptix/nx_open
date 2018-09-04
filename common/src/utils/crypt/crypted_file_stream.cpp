#include "crypted_file_stream.h"

namespace nx {
namespace utils {

CryptedFileStream::CryptedFileStream(const QString& fileName):
    m_fileName(fileName),
    m_file(fileName),
    m_mutex(QnMutex::Recursive)
{
    memset(m_currentPlainBlock, 0, kCryptoBlockSize);
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
    return m_position.position();
}

qint64 CryptedFileStream::size() const
{
    QnMutexLocker lock(&m_mutex);
    return m_header.dataSize;
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
    writeToBlock(data, toWrite);
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

bool CryptedFileStream::open(QIODevice::OpenMode openMode)
{
    // TODO: Add error-checking.
    QnMutexLocker lock(&m_mutex);
    close();

    m_file.setFileName(m_fileName);
    if (!m_file.open(openMode))
        return false;

    m_enclosure.size = m_enclosure.originalSize;

    if (m_enclosure.isNull() && openMode != WriteOnly) //< Adjust to file size except if WriteOnly.
        m_enclosure.size = m_file.size();

    // Default to whole file if no enclosure provided.
    if (!isWriting())
    {
        if (!readHeader())
            return false;
    }
    else
    {
        if (openMode == WriteOnly || !readHeader()) //< Drop header if WriteOnly or attempt to read it.
            createHeader();
    }

    m_openMode = openMode;

    QIODevice::open(openMode);
    seek(0);
    return true;
}

void CryptedFileStream::close()
{
    QnMutexLocker lock(&m_mutex);

    if (isWriting())
        flush();

    m_file.close();

    m_position = Position();
    m_header = Header();
    m_openMode = QIODevice::NotOpen;
}

void CryptedFileStream::flush()
{
    if (isWriting())
    {
        dumpCurrentBlock();
        writeHeader();
    }
}

void CryptedFileStream::readFromBlock(char* data, qint64 count)
{
    NX_ASSERT(count + m_position.positionInBlock <= kCryptoBlockSize);
    memcpy(data, m_currentPlainBlock + m_position.positionInBlock, count);
}

void CryptedFileStream::writeToBlock(const char* data, qint64 count)
{
    NX_ASSERT(count + m_position.positionInBlock <= kCryptoBlockSize);
    memcpy(m_currentPlainBlock + m_position.positionInBlock, data, count);

    m_blockDirty = true;
    m_position.positionInBlock += count;
    adjustSize();
}

void CryptedFileStream::advanceBlock()
{
    dumpCurrentBlock();
    m_position.blockIndex ++;
    loadCurrentBlock();
}

void CryptedFileStream::dumpCurrentBlock()
{
    if (!m_blockDirty || !isWriting())
        return;

    cryptBlock();
    m_file.seek(m_enclosure.position + kHeaderSize + kCryptoBlockSize * m_position.blockIndex);
    m_file.write(m_currentCryptedBlock, kCryptoBlockSize);
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
    }
}

void CryptedFileStream::createHeader()
{
    m_header = Header();
    // Create salt here.

    writeHeader();
}

bool CryptedFileStream::readHeader()
{
    if (m_enclosure.size < kHeaderSize)
        return false;
    m_file.seek(m_enclosure.position);
    return m_file.read((char *)&m_header, sizeof(m_header)) == sizeof(m_header);
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