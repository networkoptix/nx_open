# include "hanwha.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::vms::server::plugins::resource_data_support, HanwhaBypassSupportType,
    (nx::vms::server::plugins::resource_data_support::HanwhaBypassSupportType::normal, "normal")
    (nx::vms::server::plugins::resource_data_support::HanwhaBypassSupportType::forced, "forced")
    (nx::vms::server::plugins::resource_data_support::HanwhaBypassSupportType::disabled,
        "disabled")
);
