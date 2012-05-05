#include "coldstore_writer.h"
#include "coldstore_storage.h"


QnColdStoreWriter::QnColdStoreWriter(QnResourcePtr res):
QnResourceConsumer(res)
{
    
}

QnColdStoreWriter::~QnColdStoreWriter()
{

}

void QnColdStoreWriter::beforeDisconnectFromResource()
{
    pleaseStop();
}

void QnColdStoreWriter::disconnectFromResource()
{
    stop();
    QnResourceConsumer::disconnectFromResource();
}

bool QnColdStoreWriter::write(QByteArray ba, const QString& fn)
{
    return true;
}

void QnColdStoreWriter::run()
{
    if (!isConnectedToTheResource())
        return;

    while(!needToStop())
    {
        if (!isConnectedToTheResource())
            break;


    }

    m_writeQueue.clearDelete();
}

QnPlColdStoreStoragePtr QnColdStoreWriter::storage() const
{
    return getResource().dynamicCast<QnPlColdStoreStorage>();
}
