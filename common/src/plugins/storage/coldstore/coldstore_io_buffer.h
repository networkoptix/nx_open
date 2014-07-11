#ifndef cold_store_io_buffer_h_19_05
#define cold_store_io_buffer_h_19_05

#ifdef ENABLE_COLDSTORE

#include "core/resource/resource_consumer.h"

#include <QtCore/QBuffer>

class QnColdStoreIOBuffer : public QBuffer, public QnResourceConsumer
{
public:
    QnColdStoreIOBuffer(const QnResourcePtr& res, const QString& fn, int capacity = 0);
    virtual ~QnColdStoreIOBuffer();

    virtual void beforeDisconnectFromResource() override;

    virtual void close () override;

    QString getFileName() const;

private:
    QnPlColdStoreStoragePtr getColdStoreStorage() const;

private:

    QString m_fileName;
    
};

#endif // ENABLE_COLDSTORE

#endif // cold_store_io_buffer_h_19_05
