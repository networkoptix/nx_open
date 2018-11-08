#include "component.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::core::ptz, Component,
    (nx::core::ptz::Component::pan, "pan")
    (nx::core::ptz::Component::pan, "tilt")
    (nx::core::ptz::Component::pan, "rotation")
    (nx::core::ptz::Component::pan, "zoom"));

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::core::ptz, Components,
    (nx::core::ptz::Component::pan, "pan")
    (nx::core::ptz::Component::pan, "tilt")
    (nx::core::ptz::Component::pan, "rotation")
    (nx::core::ptz::Component::pan, "zoom"));
