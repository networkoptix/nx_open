// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    m_storageResource.removeFileCompletely(this);
}

bool QnLayoutCryptoStream::open(QIODevice::OpenMode openMode)
{
    // Avoid deadlock if storage is accessed from another thread
    NX_MUTEX_LOCKER storageLock(&m_storageResource.streamMutex());
    {
        // TODO: Add error-checking.
        NX_MUTEX_LOCKER lock(&m_mutex);
        close();
        if (openMode & QIODevice::WriteOnly)
        {
            if (!m_storageResource.findOrAddStream(m_streamName))
                return false;
        }

        QnLayoutFileStorageResource::Stream enclosure = m_storageResource.findStream(m_streamName);
        if (!enclosure)
            return false;

        setEnclosure(enclosure.position, enclosure.size); //< Setting crypto stream boundaries.
        if (!CryptedFileStream::open(openMode))
            return false;

        m_storageResource.registerFile(this);
        return true;
    }
}

void QnLayoutCryptoStream::close()
{
    // Avoid deadlock if storage is accessed from another thread
    NX_MUTEX_LOCKER storageLock(&m_storageResource.streamMutex());
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        const auto openMode = m_openMode;
        const bool wasWriting = isWriting();
        const qint64 totalSize = grossSize();

        CryptedFileStream::close(); //< Will change m_openMode. Unconditionally closes underlying file.

        if (openMode == NotOpen)
            return;

        if (wasWriting) // This definitely requires refactoring (VMS-11578).
            m_storageResource.finalizeWrittenStream(m_enclosure.position + totalSize);

        m_storageResource.unregisterFile(this);
    }
}

void QnLayoutCryptoStream::storeStateAndClose()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_storedPosition = m_position.position();
    m_storedOpenMode = m_openMode;
    close();
}

void QnLayoutCryptoStream::restoreState()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    open(m_storedOpenMode);
    seek(m_storedPosition);
}
