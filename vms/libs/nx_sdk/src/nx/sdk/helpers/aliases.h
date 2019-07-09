#pragma once

#include <nx/sdk/result.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_settings_response.h>
#include <nx/sdk/i_string_map.h>

namespace nx {
namespace sdk {

using VoidResult = nx::sdk::Result<void>;
using StringResult = nx::sdk::Result<const nx::sdk::IString*>;
using StringMapResult = nx::sdk::Result<const nx::sdk::IStringMap*>;
using SettingsResponseResult = nx::sdk::Result<const nx::sdk::ISettingsResponse*>;

} // namespace sdk
} // namespace nx
