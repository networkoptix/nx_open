#pragma once

#include <string>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::db {

enum class VmsResultCode
{
    ok,
    invalidData,
    networkError,
    forbidden,
    logicalError,
    unreachable,
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(VmsResultCode)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((VmsResultCode), (lexical))

struct VmsRequestResult
{
    VmsResultCode resultCode = VmsResultCode::ok;
    std::string vmsErrorDescription;
};

} // namespace nx::cloud::db
