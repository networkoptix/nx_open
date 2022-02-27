// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
            return true;   //< this has a record with a lesser key on the same position.
        if (thisIter.key() > rightIter.key())
            return false;  //< this has a record with a greater key on the same position.
        if (thisIter.value() != rightIter.value())
            return thisIter.value() < rightIter.value();
        //< right has greater sequence than this in corresponding record

        ++thisIter;
        ++rightIter;
    }
}

bool TranState::containsDataMissingIn(const TranState& right) const
{
    // TODO: #akolesnikov Replace QMap with std::map and use std::set_difference.

    auto thisIter = values.cbegin();
    auto rightIter = right.values.cbegin();

    for (;;)
    {
        if (thisIter == values.cend())
            return false;
        if (rightIter == right.values.cend())
            return true;

        if (thisIter.key() < rightIter.key())
            return true;

        if (thisIter.key() > rightIter.key())
        {
            ++rightIter;
            continue;
        }

        if (thisIter.value() > rightIter.value())
            return true;

        ++thisIter;
        ++rightIter;
    }
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TranState, (ubjson)(json)(xml), TranState_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    TranStateResponse, (ubjson)(json)(xml), TranStateResponse_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    TranSyncDoneData, (ubjson)(json)(xml), TranSyncDoneData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SyncRequestData, (ubjson)(json)(xml), SyncRequestData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
