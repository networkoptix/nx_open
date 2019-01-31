#include "tran_state_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

bool TranState::operator<(const TranState& right) const
{
    // Comparing states as sequences of pairs of comparable values.
    // So, using general string comparison rules: string str1 is considered less than str2 if it
    // contains a lesser character on some position.

    auto thisIter = values.cbegin();
    auto rightIter = right.values.cbegin();

    for (;;)
    {
        if (thisIter == values.cend() && rightIter != right.values.cend())
            return true; //< Right is longer.
        if (thisIter != values.cend() && rightIter == right.values.cend())
            return false; //< this is longer.
        if (thisIter == values.cend() && rightIter == right.values.cend())
            return false;    //< Equal.

        if (thisIter.key() < rightIter.key())
            return true;   //< this has a record with a lesser key on the some position.
        if (thisIter.key() > rightIter.key())
            return false;  //< this has a record with a greater key on the some position.
        if (thisIter.value() != rightIter.value())
            return thisIter.value() < rightIter.value();
        //< right has greater sequence than this in corresponding record

        ++thisIter;
        ++rightIter;
    }
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (TranState)(TranStateResponse)(TranSyncDoneData)(SyncRequestData),
    (eq)(ubjson)(json)(xml),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
