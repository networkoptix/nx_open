// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/aggregation_info.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API SendMailAction: public AbstractAction
{
    using base_type = AbstractAction;

public:
    explicit SendMailAction(const EventParameters& runtimeParams);

    const AggregationInfo &aggregationInfo() const;
    void setAggregationInfo(const AggregationInfo &info);

    virtual void assign(const AbstractAction& other) override;

private:
    AggregationInfo m_aggregationInfo;
};

} // namespace event
} // namespace vms
} // namespace nx
