// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multi_request_id_holder.h"

namespace nx::vms::client::core::detail {

rest::Handle MultiRequestIdHolder::setValue(Mode mode, rest::Handle value)
{
    return m_ids[static_cast<int>(mode)] = value;
}

void MultiRequestIdHolder::resetValue(Mode mode)
{
    setValue(mode, {});
}

rest::Handle MultiRequestIdHolder::value(Mode mode)
{
    return m_ids[static_cast<int>(mode)];
}

} // namespace nx::vms::client::core::detail
