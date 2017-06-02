#pragma once
/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/aggregation_info.h>

#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class SendMailAction: public AbstractAction
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