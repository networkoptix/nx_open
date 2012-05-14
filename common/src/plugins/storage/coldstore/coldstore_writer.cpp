#include "coldstore_writer.h"
#include "coldstore_storage.h"
#include "utils/common/sleep.h"


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

int QnColdStoreWriter::queueSize() const
{
    return m_writeQueue.size();
}

bool QnColdStoreWriter::write(const QByteArray& ba, const QString& fn)
{
    QnCSFile* cf = new QnCSFile();
    cf->data = ba;
    cf->fn = fn;
    
    m_writeQueue.push(cf);

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

        QnCSFile* cf;
        bool get = m_writeQueue.pop(cf, 20);

        if (!get)
        {
            QnSleep::msleep(10);
            continue;
        }

        storage()->onWrite(cf->data, cf->fn);

        delete cf;

    }

    m_writeQueue.clearDelete();
}

QnPlColdStoreStoragePtr QnColdStoreWriter::storage() const
{
    return getResource().dynamicCast<QnPlColdStoreStorage>();
}
