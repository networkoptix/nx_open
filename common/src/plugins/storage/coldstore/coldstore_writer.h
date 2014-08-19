#ifndef cold_store_writer_1838_h
#define cold_store_writer_1838_h

#ifdef ENABLE_COLDSTORE

#include "core/resource/resource_consumer.h"
#include <utils/common/threadqueue.h>
#include <utils/common/long_runnable.h>

struct QnCSFile;

class QnColdStoreWriter : public QnResourceConsumer, public QnLongRunnable
{
public:
    QnColdStoreWriter(const QnResourcePtr& res);
    virtual ~QnColdStoreWriter();

    virtual void beforeDisconnectFromResource() override;
    virtual void disconnectFromResource() override;

    bool write(const QByteArray& ba, const QString& fn);

    int queueSize() const;

protected:
    void run() override;
private:
    QnPlColdStoreStoragePtr storage() const;
private:
    typedef QSharedPointer<QnCSFile> QnCSFilePtr;
    CLThreadQueue<QnCSFilePtr> m_writeQueue;
};

#endif // ENABLE_COLDSTORE

#endif //cold_store_writer_1838_h
