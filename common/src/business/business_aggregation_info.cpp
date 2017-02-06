
#include "business_aggregation_info.h"


QnInfoDetail::QnInfoDetail()
:
    m_count(0),
    m_subAggregationData(nullptr)
{
}

QnInfoDetail::QnInfoDetail( const QnInfoDetail& right )
:
    m_runtimeParams( right.m_runtimeParams ),
    m_count( right.m_count ),
    m_subAggregationData( right.m_subAggregationData
        ? new QnBusinessAggregationInfo(*right.m_subAggregationData)
        : nullptr )
{
}

QnInfoDetail::QnInfoDetail( const QnBusinessEventParameters& _runtimeParams, int _count )
:
    m_runtimeParams( _runtimeParams ),
    m_count( _count ),
    m_subAggregationData( nullptr )
{
}

QnInfoDetail::~QnInfoDetail()
{
    delete m_subAggregationData;
    m_subAggregationData = nullptr;
}

const QnBusinessEventParameters& QnInfoDetail::runtimeParams() const
{
    return m_runtimeParams;
}

void QnInfoDetail::setRuntimeParams( const QnBusinessEventParameters& value )
{
    m_runtimeParams = value;
}

int QnInfoDetail::count() const
{
    return m_count;
}

void QnInfoDetail::setCount( int value )
{
    m_count = value;
}

void QnInfoDetail::setSubAggregationData( const QnBusinessAggregationInfo& value )
{
    if( !m_subAggregationData )
        m_subAggregationData = new QnBusinessAggregationInfo( value );
    else
        *m_subAggregationData = value;
}

const QnBusinessAggregationInfo& QnInfoDetail::subAggregationData() const
{
    if( !m_subAggregationData )
    {
        static const QnBusinessAggregationInfo emptySubAggregationData;
        return emptySubAggregationData;
    }
    return *m_subAggregationData;
}

QnInfoDetail& QnInfoDetail::operator=( const QnInfoDetail& right )
{
    if( &right == this )
        return *this;
    m_runtimeParams = right.m_runtimeParams;
    m_count = right.m_count;
    if( m_subAggregationData )
    {
        delete m_subAggregationData;
        m_subAggregationData = nullptr;
    }
    if( right.m_subAggregationData )
        m_subAggregationData = new QnBusinessAggregationInfo(*right.m_subAggregationData);
    return *this;
}



QnBusinessAggregationInfo::QnBusinessAggregationInfo() {
}


int QnBusinessAggregationInfo::totalCount() const {
    int result = 0;
    auto itr = m_details.constBegin();
    while (itr != m_details.end())
    {
        result += itr->count();
        ++itr;
    }
    return result;
}

void QnBusinessAggregationInfo::clear() {
    m_details.clear();
}

bool QnBusinessAggregationInfo::isEmpty() const {
    return m_details.isEmpty();
}

void QnBusinessAggregationInfo::append(
    const QnBusinessEventParameters &runtimeParams,
    const QnBusinessAggregationInfo& subAggregationData )
{
    QnUuid key = runtimeParams.getParamsHash();
    QnInfoDetail& info = m_details[key];
    if (info.count() == 0) //not initialized
    {
        info.setRuntimeParams(runtimeParams); //timestamp of first event is stored here
        info.setSubAggregationData( subAggregationData );
    }
    info.setCount(info.count()+1);
}

QList<QnInfoDetail> QnBusinessAggregationInfo::toList() const {
    return m_details.values();
}
