// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QDebug;

#define QN_FUSION_DECLARE_FUNCTIONS_debug(TYPE, ... /* PREFIX */)   \
__VA_ARGS__ QDebug &operator<<(QDebug &stream, const TYPE &value);  \
__VA_ARGS__ void PrintTo(const TYPE& value, ::std::ostream* os);
