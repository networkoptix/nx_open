////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifdef ENABLE_DATA_PROVIDERS

#pragma once

#include "abstract_ondemand_data_provider.h"
#include <media/filters/abstract_media_data_filter.h>

// Reads stream from specified source and performs some processing.
class FilteredOnDemandDataProvider: public AbstractOnDemandDataProvider
{
public:
    /**
     * @param dataSource MUST NOT be NULL
     */
    FilteredOnDemandDataProvider(
        const AbstractOnDemandDataProviderPtr& dataSource,
        const AbstractMediaDataFilterPtr& filter);

    //!Implementation of AbstractOnDemandDataProvider::tryRead
    /*!
    Reads data from \a dataSource, calls \a processData for read data
    */
    virtual bool tryRead(QnAbstractDataPacketPtr* const data) override;
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
    AbstractMediaDataFilterPtr m_filter;
};

#endif // ENABLE_DATA_PROVIDERS
