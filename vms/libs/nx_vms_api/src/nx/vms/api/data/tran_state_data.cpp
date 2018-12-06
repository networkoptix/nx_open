#include "tran_state_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

bool TranState::operator<(const TranState& right) const
{
    // This is less if:
    //  - right has record that is missing in this
    //  - right has greater sequence than this in corresponding record

    auto thisIter = values.cbegin();
    auto rightIter = values.cbegin();

    for (;;)
    {
        if (thisIter == values.cend() && rightIter != right.values.cend())
            return true;    //< right has record that is missing in this
        if (thisIter != values.cend() && rightIter == right.values.cend())
            return false;    //< this has record that is missing in right
        if (thisIter == values.cend() && rightIter == right.values.cend())
            return false;    //< seems like transaction states are equal

        if (thisIter.key() < rightIter.key())
            return false;   //< this has record that is missing in right
        if (thisIter.key() > rightIter.key())
            return true;    //< right has record that is missing in this
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
