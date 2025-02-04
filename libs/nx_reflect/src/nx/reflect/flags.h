// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::reflect {

enum class SerializationFlag
{
    none = 0,
    uuidWithBraces = 1 << 0,
};

} // namespace nx::reflect
