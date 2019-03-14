#include "layout_storage_filestream.h"

QnLayoutPlainStream::QnLayoutPlainStream(QnLayoutFileStorageResource& storageResource, const QString& fileName):
    m_file(storageResource.getUrl()),
    m_mutex(QnMutex::Recursive),
    m_storageResource(storageResource),
    m_fileOffset(0),
    m_fileSize(0),
    m_storedPosition(0),
    m_openMode(QIODevice::NotOpen)
{
    m_fileName = fileName.mid(fileName.lastIndexOf(QLatin1Char('?')) + 1);
}

QnLayoutPlainStream::~QnLayoutPlainStream()
{
    close();
    m_storageResource.removeFileCompletely(this);
}

bool QnLayoutPlainStream::seek(qint64 offset)
{
    QnMutexLocker lock(&m_mutex);
    // Qt docs say: "When subclassing QIODevice, you must call QIODevice::seek() at the start
    // of your function to ensure integrity with QIODevice's built-in buffer."
    QIODevice::seek(offset);
    return m_file.seek(offset + m_fileOffset);;
}

qint64 QnLayoutPlainStream::pos() const
{
    QnMutexLocker lock(&m_mutex);
    return QIODevice::pos();
}

qint64 QnLayoutPlainStream::size() const
{
    QnMutexLocker lock(&m_mutex);
    return m_fileSize;
}

qint64 QnLayoutPlainStream::readData(char* data, qint64 maxSize)
{
    QnMutexLocker lock(&m_mutex);
    return m_file.read(data, qMin(m_fileSize - pos(), maxSize));
}

qint64 QnLayoutPlainStream::writeData(const char* data, qint64 maxSize)
{
    QnMutexLocker lock(&m_mutex);
    qint64 rez = m_file.write(data, maxSize);
    if (rez > 0)
        m_fileSize = qMax(m_fileSize, m_file.pos() - m_fileOffset);
    return rez;
}

bool QnLayoutPlainStream::open(QIODevice::OpenMode openMode)
{
    QnMutexLocker storageLock(&m_storageResource.streamMutex());
    {
        QnMutexLocker lock(&m_mutex);
        m_openMode = openMode;
        if (openMode & QIODevice::WriteOnly)
        {
            if (!m_storageResource.findOrAddStream(m_fileName))
                return false;

            openMode |= QIODevice::ReadOnly;
        }
        m_file.setFileName(m_storageResource.getUrl());
        if (!m_file.open(openMode))
            return false;
        auto stream = m_storageResource.findStream(m_fileName);
        if (!stream)
            return false;
        m_fileOffset = stream.position;
        m_fileSize = stream.size;

        QIODevice::open(openMode);
        seek(0);
        m_storageResource.registerFile(this);
        return true;
    }
}

void QnLayoutPlainStream::close()
{
    QnMutexLocker storageLock(&m_storageResource.streamMutex());
    {
        QnMutexLocker lock(&m_mutex);

        QIODevice::close();
        m_file.close();

        if (m_openMode & QIODevice::WriteOnly)
            m_storageResource.finalizeWrittenStream(m_fileOffset + m_fileSize);

        m_openMode = NotOpen;

        m_storageResource.unregisterFile(this);
    }
}

void QnLayoutPlainStream::storeStateAndClose()
{
    QnMutexLocker lock(&m_mutex);
    m_storedPosition = pos();
    m_file.close();
}

void QnLayoutPlainStream::restoreState()
{
    QnMutexLocker lock(&m_mutex);
    open(m_openMode);
    seek(m_storedPosition);
}
