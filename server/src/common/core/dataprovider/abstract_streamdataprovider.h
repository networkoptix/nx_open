#ifndef QnAbstractStreamDataProvider_514
#define QnAbstractStreamDataProvider_514

#include "common/longrunnable.h"
#include "datapacket/datapacket.h"
#include "resource/resource_consumer.h"


class QnAbstractDataConsumer;


class QnAbstractStreamDataProvider : public QnLongRunnable, public QnResourceConsumer
{
public:
	explicit QnAbstractStreamDataProvider(QnResourcePtr res);
	virtual ~QnAbstractStreamDataProvider();

    virtual bool dataCanBeAccepted() const;

	void addDataProcessor(QnAbstractDataConsumer* dp);
	void removeDataProcessor(QnAbstractDataConsumer* dp);

	void pauseDataProcessors();
	void resumeDataProcessors();

    void setDataRate(qreal rate); // fps for example 
    qreal getDataRate() const;

protected:

    virtual void beforeDisconnectFromResource();

    virtual void disconnectFromResource();

    virtual void run(); 

	void putData(QnAbstractDataPacketPtr data);

    virtual QnAbstractDataPacketPtr getNextData() = 0;

    // if function returns false we do not call getNextData
    virtual bool beforeGetData() = 0;

    // if function returns false we do not put result into the queues
    virtual bool afterGetData(QnAbstractDataPacketPtr data) = 0;

    virtual void beforeRun() = 0;
    virtual void afterRun() = 0;

    virtual void sleepIfNeeded() = 0;

protected:
    mutable QMutex m_mtx;
	QList<QnAbstractDataConsumer*> m_dataprocessors;
	
    qreal m_dataRate;
};

#endif //QnAbstractStreamDataProvider_514
