////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "filtered_ondemand_data_provider.h"

#include <nx/utils/log/assert.h>

#ifdef ENABLE_DATA_PROVIDERS

FilteredOnDemandDataProvider::FilteredOnDemandDataProvider(
    const AbstractOnDemandDataProviderPtr& dataSource,
    const AbstractMediaDataFilterPtr& filter)
    :
    m_dataSource(dataSource),
    m_filter(filter)
{
    NX_ASSERT(m_dataSource);
    connect(dataSource.get(), &AbstractOnDemandDataProvider::dataAvailable,
        this, [this](AbstractOnDemandDataProvider* /*pThis*/) { emit dataAvailable(this); },
        Qt::DirectConnection);
}

bool FilteredOnDemandDataProvider::tryRead(QnAbstractDataPacketPtr* const data)
{
    if (!m_dataSource->tryRead(data))
        return false;
    if (!data)
        return true;
    *data = m_filter->processData(*data);
    return true;
}

//!Implementation of AbstractOnDemandDataProvider::currentPos
quint64 FilteredOnDemandDataProvider::currentPos() const
{
    return m_dataSource->currentPos();
}

void FilteredOnDemandDataProvider::put(QnAbstractDataPacketPtr packet)
{
    return m_dataSource->put(std::move(packet));
}

#endif // ENABLE_DATA_PROVIDERS
