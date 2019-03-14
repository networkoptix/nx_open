#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::server::plugins::resource_data_support {

enum class HanwhaBypassSupportType
{
    normal,
    forced,
    disabled
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(HanwhaBypassSupportType);

} // namespace nx::vms::server::plugins::resource_data_support

QN_FUSION_DECLARE_FUNCTIONS(
    nx::vms::server::plugins::resource_data_support::HanwhaBypassSupportType,
    (metatype)(numeric)(lexical));
