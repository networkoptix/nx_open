#include "layout_storage_cryptostream.h"

using nx::utils::CryptedFileStream;

QnLayoutCryptoStream::QnLayoutCryptoStream(QnLayoutFileStorageResource& storageResource,
    const QString& fileName, const QString& password)
    :
    CryptedFileStream(storageResource.getUrl(), password),
    m_storageResource(storageResource)

{
    m_fileName = fileName.mid(fileName.lastIndexOf(QLatin1Char('?')) + 1);
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
    bool emptyStream = false;
    if (openMode & QIODevice::WriteOnly)
    {
        if (!m_storageResource.findStream(m_fileName).valid())
        {
            if (!m_storageResource.addStream(m_fileName).valid())
                return false;

            emptyStream = true;
        }
    }

    QnLayoutFileStorageResource::Stream enclosure  = m_storageResource.findStream(m_fileName);
    if (!enclosure.valid())
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

    CryptedFileStream::close(); //< Will change m_openMode. Unconditionally closes underlying file.

    if (openMode == QIODevice::NotOpen)
        return;

    if (wasWriting)
        m_storageResource.finalizeWrittenStream();

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
