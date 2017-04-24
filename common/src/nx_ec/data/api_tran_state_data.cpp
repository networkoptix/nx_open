#include "api_tran_state_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnTranState)(QnTranStateResponse)(ApiTranSyncDoneData)(ApiSyncRequestData),
    (ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

bool operator<(const QnTranState& left, const QnTranState& right)
{
    // Left is less if:
    //  - right has record that is missing in left
    //  - right has greater sequence than left in corresponding record

    auto leftIter = left.values.cbegin();
    auto rightIter = right.values.cbegin();

    for (;;)
    {
        if (leftIter == left.values.cend() && rightIter != right.values.cend())
            return true;    //< right has record that is missing in left
        if (leftIter != left.values.cend() && rightIter == right.values.cend())
            return false;    //< left has record that is missing in right
        if (leftIter == left.values.cend() && rightIter == right.values.cend())
            return false;    //< seems like transaction states are equal

        if (leftIter.key() < rightIter.key())
            return false;   //< left has record that is missing in right
        if (leftIter.key() > rightIter.key())
            return true;    //< right has record that is missing in left
        if (leftIter.value() != rightIter.value())
            return leftIter.value() < rightIter.value();
        //< right has greater sequence than left in corresponding record

        ++leftIter;
        ++rightIter;
    }
}

bool operator>(const QnTranState& left, const QnTranState& right)
{
    return right < left;
}

} // namespace ec2
