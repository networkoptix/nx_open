#ifndef __DESKTOP_DATA_PROVIDER_WRAPPER_H
#define __DESKTOP_DATA_PROVIDER_WRAPPER_H

#include "core/dataconsumer/abstract_data_consumer.h"
#include "core/dataprovider/media_streamdataprovider.h"

/*
* This class used for sharing desktop media stream
*/

class QnDesktopDataProviderWrapper: public QnAbstractMediaStreamDataProvider, public QnAbstractDataConsumer
{
public:
    QnDesktopDataProviderWrapper(QnResourcePtr res);
    virtual ~QnDesktopDataProviderWrapper();

    virtual void start(Priority priority = InheritPriority) override;
protected:
    virtual bool processData(QnAbstractDataPacketPtr /*data*/) { return true; }
protected:
    virtual void putData(QnAbstractDataPacketPtr data) override;
    virtual void pleaseStop() override;
private:
    QSet<void*> m_needKeyData;
};

#endif // __DESKTOP_DATA_PROVIDER_WRAPPER_H
