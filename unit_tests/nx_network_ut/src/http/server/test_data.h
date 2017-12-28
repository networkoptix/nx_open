#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace network {
namespace http {
namespace server {
namespace test {

struct Serializable
{
    int dummyInt = 0;
};

#define Serializable_Fields (dummyInt)

bool serializeToHeaders(nx::network::http::HttpHeaders* where, const Serializable& what);
bool deserializeFromHeaders(const nx::network::http::HttpHeaders& from, Serializable* what);

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Serializable),
    (json))

} // namespace test
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
