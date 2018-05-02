////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ONDEMAND_MEDIA_DATA_PROVIDER_H
#define ONDEMAND_MEDIA_DATA_PROVIDER_H

#include <deque>
#include <memory>

#include <nx/utils/thread/mutex.h>
#include <QSharedPointer>

#include <providers/abstract_ondemand_data_provider.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <core/dataconsumer/abstract_data_receptor.h>


class QnAbstractStreamDataProvider;

//!Provides "pull" interface to \a QnAbstractStreamDataProvider object
class OnDemandMediaDataProvider
:
    public AbstractOnDemandDataProvider,
    public QnAbstractMediaDataReceptor
{
public:
    OnDemandMediaDataProvider( const QSharedPointer<QnAbstractStreamDataProvider>& dataProvider ) throw();
    virtual ~OnDemandMediaDataProvider() throw();

    //!Implementation of AbstractOnDemandDataProvider::tryRead
    virtual bool tryRead( QnAbstractDataPacketPtr* const data ) override;
    //!Implementation of AbstractOnDemandDataProvider::currentPos
    virtual quint64 currentPos() const override;
    //!Implementation of AbstractOnDemandDataProvider::put
    virtual void put(QnAbstractDataPacketPtr packet) override;

    //!Implementation of QnAbstractMediaDataReceptor::canAcceptData
    /*!
        \return false if internal buffer is full. true, otherwise
    */
    virtual bool canAcceptData() const override;
    //!Implementation of QnAbstractMediaDataReceptor::putData
    /*!
        Stores \a data in internal buffer, which is read out by \a tryRead
    */
    virtual void putData( const QnAbstractDataPacketPtr& data ) override;

private:
    QSharedPointer<QnAbstractStreamDataProvider> m_dataProvider;
    mutable QnMutex m_mutex;
    std::deque<QnAbstractDataPacketPtr> m_dataQueue;
    qint64 m_prevPacketTimestamp;
};

typedef std::shared_ptr<OnDemandMediaDataProvider> OnDemandMediaDataProviderPtr;

#endif  //ONDEMAND_MEDIA_DATA_PROVIDER_H
