////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ONDEMAND_MEDIA_DATA_PROVIDER_H
#define ONDEMAND_MEDIA_DATA_PROVIDER_H

#include <memory>
#include <queue>

#include <QMutex>
#include <QSharedPointer>

#include <core/dataprovider/abstract_ondemand_data_provider.h>
#include <core/dataprovider/abstract_streamdataprovider.h>
#include <core/dataconsumer/abstract_data_receptor.h>


class QnAbstractStreamDataProvider;

//!Provides "pull" interface to \a QnAbstractStreamDataProvider object
class OnDemandMediaDataProvider
:
    public AbstractOnDemandDataProvider,
    public QnAbstractDataReceptor
{
public:
    OnDemandMediaDataProvider( const QSharedPointer<QnAbstractStreamDataProvider>& dataProvider ) throw();
    virtual ~OnDemandMediaDataProvider() throw();

    //!Implementation of AbstractOnDemandDataProvider::tryRead
    virtual bool tryRead( QnAbstractDataPacketPtr* const data ) override;
    //!Implementation of AbstractOnDemandDataProvider::currentPos
    virtual quint64 currentPos() const override;

    //!Implementation of QnAbstractDataReceptor::canAcceptData
    /*!
        \return false if internal buffer is full. true, otherwise
    */
    virtual bool canAcceptData() const override;
    //!Implementation of QnAbstractDataReceptor::putData
    /*!
        Stores \a data in internal buffer, which is read out by \a tryRead
    */
    virtual void putData( const QnAbstractDataPacketPtr& data ) override;

private:
    QSharedPointer<QnAbstractStreamDataProvider> m_dataProvider;
    mutable QMutex m_mutex;
    std::queue<QnAbstractDataPacketPtr> m_dataQueue;
    qint64 m_prevPacketTimestamp;
};

typedef std::shared_ptr<OnDemandMediaDataProvider> OnDemandMediaDataProviderPtr;

#endif  //ONDEMAND_MEDIA_DATA_PROVIDER_H
