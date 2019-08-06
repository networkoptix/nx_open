// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/result.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_settings_response.h>
#include <nx/sdk/i_string_map.h>

namespace nx {
namespace sdk {

using StringResult = Result<const IString*>;
using StringMapResult = Result<const IStringMap*>;
using SettingsResponseResult = Result<const ISettingsResponse*>;

} // namespace sdk
} // namespace nx
