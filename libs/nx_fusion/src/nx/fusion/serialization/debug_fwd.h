#pragma once

class QDebug;

#define QN_FUSION_DECLARE_FUNCTIONS_debug(TYPE, ... /* PREFIX */)   \
__VA_ARGS__ QDebug &operator<<(QDebug &stream, const TYPE &value);  \
__VA_ARGS__ void PrintTo(const TYPE& value, ::std::ostream* os);    \
