////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACT_MEDIA_DATA_FILTER_H
#define ABSTRACT_MEDIA_DATA_FILTER_H

#ifdef ENABLE_DATA_PROVIDERS

#include "abstract_ondemand_data_provider.h"


//!Reads stream from specified source and performs some processing
class AbstractMediaDataFilter
:
    public AbstractOnDemandDataProvider
{
public:
    /*!
        \param dataSource MUST NOT be NULL
    */
    AbstractMediaDataFilter( const AbstractOnDemandDataProviderPtr& dataSource );

    //!Implementation of AbstractOnDemandDataProvider::tryRead
    /*!
        Reads data from \a dataSource, calls \a processData for read data
    */
    virtual bool tryRead( QnAbstractDataPacketPtr* const data ) override;
    //!Implementation of AbstractOnDemandDataProvider::currentPos
    /*!
        Just delegates call to \a dataSource
    */
    virtual quint64 currentPos() const override;
    /*!
        Just delegates call to \a dataSource
    */
    virtual void put(QnAbstractDataPacketPtr packet) override;

protected:
    AbstractOnDemandDataProviderPtr m_dataSource;

    /*!
        Whether to copy source data or perform in-place processing is up to implementation
        \param data Source data
        \return Modified data. Can be \a data
    */
    virtual QnAbstractDataPacketPtr processData( QnAbstractDataPacketPtr* const data ) = 0;
};

#endif // ENABLE_DATA_PROVIDERS

#endif  //ABSTRACT_MEDIA_DATA_FILTER_H
