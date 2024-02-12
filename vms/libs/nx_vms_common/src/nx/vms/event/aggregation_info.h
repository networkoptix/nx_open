// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/event_parameters.h>

namespace nx {
namespace vms {
namespace event {

class AggregationInfo;

class NX_VMS_COMMON_API InfoDetail
{
public:
    InfoDetail();
    InfoDetail(const InfoDetail& right);
    InfoDetail(const EventParameters& runtimeParams, int count);
    ~InfoDetail();

    const EventParameters& runtimeParams() const;
    void setRuntimeParams(const EventParameters& value);
    int count() const;
    void setCount(int value);
    void setSubAggregationData(const AggregationInfo& value);
    const AggregationInfo& subAggregationData() const;

    InfoDetail& operator=(const InfoDetail& right);

private:
    EventParameters m_runtimeParams;
    int m_count = 0;
    AggregationInfo* m_subAggregationData = nullptr;
};

class NX_VMS_COMMON_API AggregationInfo
{
public:
    AggregationInfo();

    void append(const EventParameters& runtimeParams,
        const AggregationInfo& subAggregationData = AggregationInfo(),
        bool oneRecordPerKey = false);
    bool isEmpty() const;
    int totalCount() const;
    void clear();

    QList<InfoDetail> toList() const;

private:
    QMap<nx::Uuid, InfoDetail> m_details;
};

} // namespace event
} // namespace vms
} // namespace nx
