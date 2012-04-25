#ifndef cold_store_writer_1838_h
#define cold_store_writer_1838_h

#include "core/resource/resource_consumer.h"
#include "utils/common/longrunnable.h"

class QnCSFile;

class QnColdStoreWriter : public QnResourceConsumer, public CLLongRunnable
{
public:
    QnColdStoreWriter(QnResourcePtr res);
    virtual ~QnColdStoreWriter();

    virtual void beforeDisconnectFromResource() override;
    virtual void disconnectFromResource() override;

    bool write(QByteArray ba, const QString& fn);

protected:
    void run() override;
private:
    QnPlColdStoreStoragePtr storage() const;
private:
    CLThreadQueue<QnCSFile*> m_writeQueue;
};

#endif //cold_store_writer_1838_h