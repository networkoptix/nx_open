// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "send_mail_action.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace vms {
namespace event {

SendMailAction::SendMailAction(const EventParameters& runtimeParams):
    base_type(ActionType::sendMailAction, runtimeParams)
{
}

const AggregationInfo& SendMailAction::aggregationInfo() const
{
    return m_aggregationInfo;
}

void SendMailAction::setAggregationInfo(const AggregationInfo& info)
{
    m_aggregationInfo = info;
}

void SendMailAction::assign(const AbstractAction& other)
{
    base_type::assign(other);

    if (other.actionType() != ActionType::sendMailAction)
        return;

    const auto otherSendMail = dynamic_cast<const SendMailAction*>(&other);
    NX_ASSERT(otherSendMail);
    if (!otherSendMail)
        return;

    m_aggregationInfo = otherSendMail->m_aggregationInfo;
}

} // namespace event
} // namespace vms
} // namespace nx
