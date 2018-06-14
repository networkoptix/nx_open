#pragma once

#include <nx/core/ptz/type.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace core {
namespace ptz {

struct Options
{
    Type type = Type::operational;
};
#define PtzOptions_Fields (type)
QN_FUSION_DECLARE_FUNCTIONS(Options, (json))

} // namespace ptz
} // namespace core
} // namespace nx
