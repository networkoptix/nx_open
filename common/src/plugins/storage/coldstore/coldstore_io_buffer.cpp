#include "coldstore_io_buffer.h"
#include "coldstore_storage.h"


QnColdStoreIOBuffer::QnColdStoreIOBuffer(QnResourcePtr res, const QString& fn, int capacity):
QnResourceConsumer(res),
m_fileName(fn)
{
    if (capacity)
        buffer().reserve(capacity);
}

QnColdStoreIOBuffer::~QnColdStoreIOBuffer()
{
    disconnectFromResource();
}

void QnColdStoreIOBuffer::beforeDisconnectFromResource()
{
    disconnectFromResource();
}

void QnColdStoreIOBuffer::close()
{
    if (!isConnectedToTheResource())
        return;

    if (openMode() == QIODevice::WriteOnly)
        getColdStoreStorage()->onWriteBuffClosed(this);

    QBuffer::close();
}

QString QnColdStoreIOBuffer::getFileName() const
{
    return m_fileName;
}

//==========================================================================================
QnPlColdStoreStoragePtr QnColdStoreIOBuffer::getColdStoreStorage() const
{
    return getResource().dynamicCast<QnPlColdStoreStorage>();
}