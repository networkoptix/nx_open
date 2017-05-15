#include "ptz_constants.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Ptz, Capabilities)

// TODO: #Elric #2.3 code duplication ^v
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Ptz, Trait,
    (Ptz::FourWayPtzTrait, "FourWayPtz")
    (Ptz::EightWayPtzTrait, "EightWayPtz")
    (Ptz::ManualAutoFocusPtzTrait, "ManualAutoFocus"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Ptz, Traits,
    (Ptz::FourWayPtzTrait, "FourWayPtz")
    (Ptz::EightWayPtzTrait, "EightWayPtz")
    (Ptz::ManualAutoFocusPtzTrait, "ManualAutoFocus"))
