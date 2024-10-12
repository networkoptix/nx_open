// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

enum class MultiThreadDecodePolicy
{
    autoDetect, //< off for small resolution, on for large resolution
    disabled,
    enabled
};
