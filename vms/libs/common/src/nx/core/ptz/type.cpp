#include "type.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::core::ptz, Type,
    (nx::core::ptz::Type::operational, "operational")
    (nx::core::ptz::Type::configurational, "configurational")
);

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::core::ptz, Types,
    (nx::core::ptz::Type::operational, "operational")
    (nx::core::ptz::Type::configurational, "configurational")
);
