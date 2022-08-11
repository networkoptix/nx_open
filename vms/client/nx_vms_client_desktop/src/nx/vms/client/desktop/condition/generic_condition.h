// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/condition/condition_types.h>

namespace nx::vms::client::desktop {

class GenericCondition
{
public:
    template<class Item, class ItemSequence>
    static bool check(const ItemSequence& sequence,
        MatchMode match,
        std::function<bool(const Item& item)> checkOne)
    {
        switch (match)
        {
            case MatchMode::any:
                return std::any_of(sequence.cbegin(), sequence.cend(), checkOne);
            case MatchMode::all:
                return std::all_of(sequence.cbegin(), sequence.cend(), checkOne);
            case MatchMode::exactlyOne:
                return (std::count_if(sequence.cbegin(), sequence.cend(), checkOne) == 1);
            case MatchMode::none:
                return std::none_of(sequence.cbegin(), sequence.cend(), checkOne);
            default:
                break;
        }
        NX_ASSERT(false, "Invalid match mode '%1'.", static_cast<int>(match));
        return false;
    }

    template<class Item, class ItemSequence>
    static ConditionResult check(const ItemSequence& sequence,
        std::function<bool(const Item& item)> checkOne)
    {
        int count = std::count_if(sequence.cbegin(), sequence.cend(), checkOne);

        if (count == 0)
            return ConditionResult::None;

        if (count == sequence.size())
            return ConditionResult::All;

        return ConditionResult::Partial;
    }

};

} // namespace nx::vms::client::desktop
