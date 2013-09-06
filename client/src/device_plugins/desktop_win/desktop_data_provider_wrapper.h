#ifndef __DESKTOP_DATA_PROVIDER_WRAPPER_H
#define __DESKTOP_DATA_PROVIDER_WRAPPER_H

#include "core/dataconsumer/abstract_data_consumer.h"
#include "core/dataprovider/media_streamdataprovider.h"

class QnDesktopDataProvider;

/*
* This class used for sharing desktop media stream
*/

class QnDesktopDataProviderWrapper: public QnAbstractMediaStreamDataProvider, public QnAbstractDataConsumer
{
public:
    QnDesktopDataProviderWrapper(QnResourcePtr res, QnDesktopDataProvider* owner);
    virtual ~QnDesktopDataProviderWrapper();

    virtual void start(Priority priority = InheritPriority) override;
    bool isInitialized() const;
    QString lastErrorStr() const;
    virtual bool hasThread() const override { return false; }
protected:
    virtual bool processData(QnAbstractDataPacketPtr /*data*/) { return true; }
protected:
    virtual void putData(QnAbstractDataPacketPtr data) override;
private:
    QSet<void*> m_needKeyData;
    QnDesktopDataProvider* m_owner;
};

#endif // __DESKTOP_DATA_PROVIDER_WRAPPER_H
