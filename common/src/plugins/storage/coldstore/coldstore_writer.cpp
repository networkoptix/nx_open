#include "coldstore_writer.h"

#ifdef ENABLE_COLDSTORE

#include "coldstore_storage_helper.h"
#include "coldstore_storage.h"
#include "utils/common/sleep.h"


QnColdStoreWriter::QnColdStoreWriter(const QnResourcePtr& res):
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
    QnCSFilePtr cf = QnCSFilePtr(new QnCSFile());
    cf->data = ba;
    cf->fn = fn;
    
    m_writeQueue.push(cf);

    return true;
}

void QnColdStoreWriter::run()
{
    initSystemThreadId();
    if (!isConnectedToTheResource())
        return;

    while(!needToStop())
    {
        if (!isConnectedToTheResource())
            break;

        QnCSFilePtr cf;
        bool get = m_writeQueue.pop(cf, 20);

        if (!get)
        {
            QnSleep::msleep(10);
            continue;
        }

        storage()->onWrite(cf->data, cf->fn);


    }

    m_writeQueue.clear();
}

QnPlColdStoreStoragePtr QnColdStoreWriter::storage() const
{
    return getResource().dynamicCast<QnPlColdStoreStorage>();
}

#endif // ENABLE_COLDSTORE
