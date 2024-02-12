// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_lookup_list_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

Result AbstractLookupListManager::getLookupListsSync(
    nx::vms::api::LookupListDataList* outDataList)
{
    return detail::callSync(
        [&](auto handler)
        {
            getLookupLists(std::move(handler));
        },
        outDataList);
}

Result AbstractLookupListManager::saveSync(const nx::vms::api::LookupListData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            save(data, std::move(handler));
        });
}

Result AbstractLookupListManager::removeSync(const nx::Uuid& id)
{
    return detail::callSync(
        [&](auto handler)
        {
            remove(id, std::move(handler));
        });
}

} // namespace ec2
