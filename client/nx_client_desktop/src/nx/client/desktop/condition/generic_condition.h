#pragma once

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/algorithm/count_if.hpp>

#include <nx/client/desktop/condition/condition_types.h>

namespace nx {
namespace client {
namespace desktop {

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
            case MatchMode::Any:
                return boost::algorithm::any_of(sequence, checkOne);
            case MatchMode::All:
                return boost::algorithm::all_of(sequence, checkOne);
            case MatchMode::ExactlyOne:
                return (boost::count_if(sequence, checkOne) == 1);
            default:
                break;
        }
        NX_EXPECT(false, lm("Invalid match mode '%1'.").arg(static_cast<int>(match)));
        return false;
//
//         int count = 0;
//         for (const Item& item: sequence)
//         {
//             bool matches = checkOne(item);
//
//             if (matches && match == MatchMode::Any)
//                 return true;
//
//             if (!matches && match == MatchMode::All)
//                 return false;
//
//             if (matches)
//                 count++;
//         }
//
//         if (match == MatchMode::Any)
//             return false;
//
//         if (match == MatchMode::All)
//             return true;
//
//         if (match == MatchMode::ExactlyOne)
//             return count == 1;
//
//         NX_EXPECT(false, lm("Invalid match mode '%1'.").arg(static_cast<int>(match)));
//         return false;
    }

    template<class Item, class ItemSequence>
    static ConditionResult check(const ItemSequence& sequence,
        std::function<bool(const Item& item)> checkOne)
    {
        int count = boost::count_if(sequence, checkOne);
        int total = sequence.size();
//         for (const Item& item: sequence)
//         {
//             ++total;
//             if (checkOne(item))
//                 count++;
//         }

        if (count == 0)
            return ConditionResult::None;

        if (count == total)
            return ConditionResult::All;

        return ConditionResult::Partial;
    }

};

} // namespace desktop
} // namespace client
} // namespace nx