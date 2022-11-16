// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_data.h"

namespace nx::network::http::test {

namespace {

static constexpr const char* testHeaderName = "Test-Serializable-dummyInt";

} // namespace

bool serializeToHeaders(nx::network::http::HttpHeaders* where, const Serializable& what)
{
    where->emplace(testHeaderName, std::to_string(what.dummyInt));
    return true;
}

bool deserializeFromHeaders(const nx::network::http::HttpHeaders& from, Serializable* what)
{
    auto it = from.find(testHeaderName);
    if (it == from.end())
        return false;

    what->dummyInt = nx::utils::stoi(it->second);
    return true;
}

} // namespace nx::network::http::test
