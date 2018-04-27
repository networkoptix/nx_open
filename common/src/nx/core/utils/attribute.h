#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace core {
namespace utils {

struct Attribute
{
    QString name;
    QString value;
};

using AttributeList = std::vector<Attribute>;

QN_FUSION_DECLARE_FUNCTIONS(Attribute, (json));

} // namespace utils
} // namespace core
} // namespace nx
