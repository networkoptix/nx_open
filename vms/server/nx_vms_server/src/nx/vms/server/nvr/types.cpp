#include "types.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::nvr, FanState,
    (nx::vms::server::nvr::FanState::undefined, "")
    (nx::vms::server::nvr::FanState::ok, "ok")
    (nx::vms::server::nvr::FanState::error, ""))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::nvr, BuzzerState,
    (nx::vms::server::nvr::BuzzerState::undefined, "")
    (nx::vms::server::nvr::BuzzerState::disabled, "disabled")
    (nx::vms::server::nvr::BuzzerState::enabled, "enabled"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::nvr, IoPortState,
    (nx::vms::server::nvr::IoPortState::undefined, "")
    (nx::vms::server::nvr::IoPortState::inactive, "inactive")
    (nx::vms::server::nvr::IoPortState::active, "active"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::nvr, LedState,
    (nx::vms::server::nvr::LedState::undefined, "")
    (nx::vms::server::nvr::LedState::disabled, "disabled")
    (nx::vms::server::nvr::LedState::enabled, "enabled"))


namespace nx::vms::server::nvr {

QString toString(FanState state) { return QnLexical::serialized(state); }

QString toString(BuzzerState state) { return QnLexical::serialized(state); }

QString toString(IoPortState state) { return QnLexical::serialized(state); }

QString toString(LedState state) { return QnLexical::serialized(state); }

} // namespace nx::vms::server::nvr
