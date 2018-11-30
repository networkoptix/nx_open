# include "hanwha.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::mediaserver_core::plugins::resource_data_support, HanwhaBypassSupportType,
    (nx::mediaserver_core::plugins::resource_data_support::HanwhaBypassSupportType::normal, "normal")
    (nx::mediaserver_core::plugins::resource_data_support::HanwhaBypassSupportType::forced, "forced")
    (nx::mediaserver_core::plugins::resource_data_support::HanwhaBypassSupportType::disabled,
        "disabled")
);
