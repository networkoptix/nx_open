#include "coldstore_io_buffer.h"


QnColdStoreIOBuffer::QnColdStoreIOBuffer(QnResourcePtr res, const QString& fn, int capacity):
QnResourceConsumer(res),
m_fileName(fn)
{
    static int n = 0;
    ++n;

    m_n = n;


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

    int size = buffer().size();

    size = size;
}
