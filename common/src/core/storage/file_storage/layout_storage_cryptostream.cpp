#include "layout_storage_cryptostream.h"

QnLayoutCryptoStream::QnLayoutCryptoStream(QnLayoutFileStorageResource& storageResource, const QString& fileName):
    m_storageResource(storageResource),
    m_file(storageResource.getUrl()),
    m_mutex(QnMutex::Recursive)

{
    m_fileName = fileName.mid(fileName.lastIndexOf(QLatin1Char('?')) + 1);
}

QnLayoutCryptoStream::~QnLayoutCryptoStream()
{
    close();
}

bool QnLayoutCryptoStream::seek(qint64 offset)
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

qint64 QnLayoutCryptoStream::pos() const
{
    QnMutexLocker lock(&m_mutex);
    return m_position.position();
}

qint64 QnLayoutCryptoStream::size() const
{
    QnMutexLocker lock(&m_mutex);
    return m_header.dataSize;
}

qint64 QnLayoutCryptoStream::readData(char* data, qint64 maxSize)
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

qint64 QnLayoutCryptoStream::writeData(const char* data, qint64 maxSize)
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

bool QnLayoutCryptoStream::open(QIODevice::OpenMode openMode)
{
    // TODO: Add error-checking.
    QnMutexLocker lock(&m_mutex);
    close();
    bool emptyStream = false;
    if (openMode & QIODevice::WriteOnly)
    {
        if (!m_storageResource.findStream(m_fileName).valid())
        {
            if (!m_storageResource.addStream(m_fileName).valid())
                return false;

            emptyStream = true;
        }
        openMode |= QIODevice::ReadOnly;
    }

    m_file.setFileName(m_storageResource.getUrl());
    if (!m_file.open(openMode))
        return false;

    m_enclosure  = m_storageResource.findStream(m_fileName);
    if (!m_enclosure.valid())
        return false;

    if (emptyStream)
        createHeader();
    else
        readHeader();

    m_openMode = openMode;

    QIODevice::open(openMode);
    seek(0);
    m_storageResource.registerFile(this);
    return true;
}

void QnLayoutCryptoStream::close()
{
    QnMutexLocker lock(&m_mutex);

    if (m_openMode == QIODevice::NotOpen)
        return;

    flush();

    m_file.close();
    if (isWriting())
        m_storageResource.finalizeWrittenStream();

    m_position = Position();
    m_openMode = QIODevice::NotOpen;

    m_storageResource.unregisterFile(this);
}

void QnLayoutCryptoStream::lockFile()
{
    m_mutex.lock();
}

void QnLayoutCryptoStream::unlockFile()
{
    m_mutex.unlock();
}

void QnLayoutCryptoStream::storeStateAndClose()
{
    flush();
    m_storedPosition = m_position.position();
    m_storedOpenMode = m_openMode;
    close();
}

void QnLayoutCryptoStream::restoreState()
{
    open(m_storedOpenMode);
    seek(m_storedPosition);
}

void QnLayoutCryptoStream::flush()
{
    if (isWriting())
    {
        dumpCurrentBlock();
        writeHeader();
    }
}

void QnLayoutCryptoStream::readFromBlock(char* data, qint64 count)
{
    NX_ASSERT(count + m_position.positionInBlock <= kCryptoBlockSize);
    memcpy(data, m_currentPlainBlock + m_position.positionInBlock, count);
}

void QnLayoutCryptoStream::writeToBlock(const char* data, qint64 count)
{
    NX_ASSERT(count + m_position.positionInBlock <= kCryptoBlockSize);
    memcpy(m_currentPlainBlock + m_position.positionInBlock, data, count);

    m_blockDirty = true;
    m_position.positionInBlock += count;
    adjustSize();
}

void QnLayoutCryptoStream::advanceBlock()
{
    dumpCurrentBlock();
    m_position.blockIndex ++;
    loadCurrentBlock();
}

void QnLayoutCryptoStream::dumpCurrentBlock()
{
    if (!m_blockDirty || !isWriting())
        return;

    cryptBlock();
    m_file.seek(m_enclosure.position + kHeaderSize + kCryptoBlockSize * m_position.blockIndex);
    m_file.write(m_currentCryptedBlock, kCryptoBlockSize);
}

void QnLayoutCryptoStream::loadCurrentBlock()
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

void QnLayoutCryptoStream::createHeader()
{
    m_header = Header();
    // Create salt here.

    writeHeader();
}

void QnLayoutCryptoStream::readHeader()
{
    m_file.seek(m_enclosure.position);
    m_file.read((char *)&m_header, kHeaderSize);
}

void QnLayoutCryptoStream::writeHeader()
{
    m_file.seek(m_enclosure.position);
    m_file.write((char *)&m_header, kHeaderSize);
}

void QnLayoutCryptoStream::cryptBlock()
{
    memcpy(m_currentCryptedBlock, m_currentPlainBlock,  kCryptoBlockSize);
}

void QnLayoutCryptoStream::decryptBlock()
{
    memcpy(m_currentPlainBlock, m_currentCryptedBlock, kCryptoBlockSize);
}
