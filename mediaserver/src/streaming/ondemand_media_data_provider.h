////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ONDEMAND_MEDIA_DATA_PROVIDER_H
#define ONDEMAND_MEDIA_DATA_PROVIDER_H

#include <QSharedPointer>

#include <core/dataprovider/abstract_ondemand_data_provider.h>
#include <core/dataprovider/abstract_streamdataprovider.h>


class QnAbstractStreamDataProvider;

//!Provides "pull" inteface to \a QnAbstractStreamDataProvider object
class OnDemandMediaDataProvider
:
    public AbstractOnDemandDataProvider
{
public:
    OnDemandMediaDataProvider( const QSharedPointer<QnAbstractStreamDataProvider>& dataProvider ) throw();

    //!Implementation of AbstractOnDemandDataProvider::tryRead
    virtual bool tryRead( QnAbstractDataPacketPtr* const data ) override;

private:
    QSharedPointer<QnAbstractStreamDataProvider> m_dataProvider;
};

#endif  //ONDEMAND_MEDIA_DATA_PROVIDER_H
