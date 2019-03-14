#include "test_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace network {
namespace http {
namespace server {
namespace test {

namespace {

static constexpr const char* testHeaderName = "Test-Serializable-dummyInt";

} // namespace

bool serializeToHeaders(nx::network::http::HttpHeaders* where, const Serializable& what)
{
    where->emplace(
        testHeaderName,
        nx::network::http::StringType::number(what.dummyInt));
    return true;
}

bool deserializeFromHeaders(const nx::network::http::HttpHeaders& from, Serializable* what)
{
    auto it = from.find(testHeaderName);
    if (it == from.end())
        return false;

    what->dummyInt = it->second.toInt();
    return true;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Serializable),
    (json),
    _Fields)

} // namespace test
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
