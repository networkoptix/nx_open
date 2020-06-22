#include "types.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::server::analytics, SettingsOrigin,
    (nx::vms::server::analytics::SettingsOrigin::server, "server")
    (nx::vms::server::analytics::SettingsOrigin::plugin, "plugin"))

namespace nx::vms::server::analytics {

QString toString(SettingsOrigin settingsOrigin) { return QnLexical::serialized(settingsOrigin); }

} // namespace nx::vms::server::analytics
