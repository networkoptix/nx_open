////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACTONDEMANDDATAPROVIDER_H
#define ABSTRACTONDEMANDDATAPROVIDER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <memory>

#include <QObject>

#include "../datapacket/abstract_data_packet.h"


//!Interface of class, providing data on demand (pull mode)
class AbstractOnDemandDataProvider
:
    public QObject
{
    Q_OBJECT

public:
    virtual ~AbstractOnDemandDataProvider() {}

    /*!
        \return true, if packet is returned, false otherwise
        \note End-of-data signalled by returning NULL packet and returning \a true
    */
    virtual bool tryRead( QnAbstractDataPacketPtr* const data ) = 0;
    //!Returns current timestamp (in micros)
    virtual quint64 currentPos() const = 0;

signals:
    //!
    /*!
        It is recommended to connect to this signal directly
        \note 
    */
    void dataAvailable( AbstractOnDemandDataProvider* pThis );
};

typedef std::shared_ptr<AbstractOnDemandDataProvider> AbstractOnDemandDataProviderPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif  //ABSTRACTONDEMANDDATAPROVIDER_H
