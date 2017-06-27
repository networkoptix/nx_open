#include "action_parameters.h"

#include <nx/fusion/model_functions.h>

namespace {
const int kDefaultFixedDurationMs = 5000;
const int kDefaultRecordBeforeMs = 1000;
} // namespace

namespace nx {
namespace vms {
namespace event {

ActionParameters::ActionParameters():
    actionResourceId(),
    url(),
    emailAddress(),
    fps(10),
    streamQuality(Qn::QualityHighest),
    recordAfter(0),
    relayOutputId(),
    sayText(),
    tags(),
    text(),
    durationMs(kDefaultFixedDurationMs),
    additionalResources(),
    allUsers(false),
    forced(true),
    presetId(),
    useSource(false),
    recordBeforeMs(kDefaultRecordBeforeMs),
    playToClient(true)
{
}

bool ActionParameters::isDefault() const
{
    static ActionParameters empty;
    return (*this) == empty;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ActionParameters), (ubjson)(json)(eq)(xml)(csv_record), _Fields, (brief, true))

} // namespace event
} // namespace vms
} // namespace nx
