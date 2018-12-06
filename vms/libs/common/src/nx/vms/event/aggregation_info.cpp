#include "aggregation_info.h"

namespace nx {
namespace vms {
namespace event {

InfoDetail::InfoDetail():
    m_count(0),
    m_subAggregationData(nullptr)
{
}

InfoDetail::InfoDetail(const InfoDetail& right):
    m_runtimeParams(right.m_runtimeParams),
    m_count(right.m_count),
    m_subAggregationData(right.m_subAggregationData
        ? new AggregationInfo(*right.m_subAggregationData)
        : nullptr)
{
}

InfoDetail::InfoDetail(const EventParameters& runtimeParams, int count):
    m_runtimeParams(runtimeParams),
    m_count(count),
    m_subAggregationData(nullptr)
{
}

InfoDetail::~InfoDetail()
{
    delete m_subAggregationData;
    m_subAggregationData = nullptr;
}

const EventParameters& InfoDetail::runtimeParams() const
{
    return m_runtimeParams;
}

void InfoDetail::setRuntimeParams(const EventParameters& value)
{
    m_runtimeParams = value;
}

int InfoDetail::count() const
{
    return m_count;
}

void InfoDetail::setCount(int value)
{
    m_count = value;
}

void InfoDetail::setSubAggregationData(const AggregationInfo& value)
{
    if (!m_subAggregationData)
        m_subAggregationData = new AggregationInfo(value);
    else
        *m_subAggregationData = value;
}

const AggregationInfo& InfoDetail::subAggregationData() const
{
    if (!m_subAggregationData)
    {
        static const AggregationInfo emptySubAggregationData;
        return emptySubAggregationData;
    }
    return *m_subAggregationData;
}

InfoDetail& InfoDetail::operator=(const InfoDetail& right)
{
    if (&right == this)
        return *this;

    m_runtimeParams = right.m_runtimeParams;
    m_count = right.m_count;

    if (m_subAggregationData)
    {
        delete m_subAggregationData;
        m_subAggregationData = nullptr;
    }

    if (right.m_subAggregationData)
        m_subAggregationData = new AggregationInfo(*right.m_subAggregationData);

    return *this;
}

AggregationInfo::AggregationInfo()
{
}

int AggregationInfo::totalCount() const
{
    int result = 0;
    for (const auto& detail: m_details)
        result += detail.count();
    return result;
}

void AggregationInfo::clear()
{
    m_details.clear();
}

bool AggregationInfo::isEmpty() const
{
    return m_details.isEmpty();
}

void AggregationInfo::append(
    const EventParameters& runtimeParams,
    const AggregationInfo& subAggregationData)
{
    QnUuid key = runtimeParams.getParamsHash();
    InfoDetail& info = m_details[key];
    if (info.count() == 0) //not initialized
    {
        info.setRuntimeParams(runtimeParams); //timestamp of first event is stored here
        info.setSubAggregationData(subAggregationData);
    }
    info.setCount(info.count()+1);
}

QList<InfoDetail> AggregationInfo::toList() const
{
    return m_details.values();
}

} // namespace event
} // namespace vms
} // namespace nx
