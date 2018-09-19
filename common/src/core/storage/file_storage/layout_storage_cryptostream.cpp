#include "layout_storage_cryptostream.h"

using nx::utils::CryptedFileStream;

QnLayoutCryptoStream::QnLayoutCryptoStream(QnLayoutFileStorageResource& storageResource,
    const QString& fileName, const QString& password)
    :
    CryptedFileStream(storageResource.getUrl(), password),
    m_storageResource(storageResource)

{
    m_streamName = fileName.mid(fileName.lastIndexOf(QLatin1Char('?')) + 1);
}

QnLayoutCryptoStream::~QnLayoutCryptoStream()
{
    close();
}

bool QnLayoutCryptoStream::open(QIODevice::OpenMode openMode)
{
    // TODO: Add error-checking.
    QnMutexLocker lock(&m_mutex);
    close();
    if (openMode & QIODevice::WriteOnly)
    {
        if (!m_storageResource.findOrAddStream(m_streamName))
            return false;
    }

    QnLayoutFileStorageResource::Stream enclosure  = m_storageResource.findStream(m_streamName);
    if (!enclosure)
        return false;

    setEnclosure(enclosure.position, enclosure.size); //< Setting crypto stream boundaries.
    if (!CryptedFileStream::open(openMode))
        return false;

    m_storageResource.registerFile(this);
    return true;
}

void QnLayoutCryptoStream::close()
{
    QnMutexLocker lock(&m_mutex);

    const auto openMode = m_openMode;
    const bool wasWriting = isWriting();
    const qint64 totalSize = grossSize();

    CryptedFileStream::close(); //< Will change m_openMode. Unconditionally closes underlying file.

    if (openMode == NotOpen)
        return;

    if (wasWriting) // This definetly requires refactoring (VMS-11578).
        m_storageResource.finalizeWrittenStream(m_enclosure.position + totalSize);

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
    m_storedPosition = m_position.position();
    m_storedOpenMode = m_openMode;
    close();
}

void QnLayoutCryptoStream::restoreState()
{
    open(m_storedOpenMode);
    seek(m_storedPosition);
}
