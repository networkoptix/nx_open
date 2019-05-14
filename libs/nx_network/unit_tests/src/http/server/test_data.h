#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>

namespace nx::network::http::test {

struct Serializable
{
    int dummyInt = 0;

    bool operator==(const Serializable& right) const
    {
        return dummyInt == right.dummyInt;
    }
};

#define Serializable_Fields (dummyInt)

bool serializeToHeaders(nx::network::http::HttpHeaders* where, const Serializable& what);
bool deserializeFromHeaders(const nx::network::http::HttpHeaders& from, Serializable* what);

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Serializable),
    (json))

} // namespace nx::network::http::test
