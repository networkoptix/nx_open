// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <string_view>

class QColor;

NX_VMS_COMMON_API std::string toString(const QColor& value);
NX_VMS_COMMON_API bool fromString(const std::string_view& str, QColor* value);
