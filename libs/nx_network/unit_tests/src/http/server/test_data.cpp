// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_data.h"

#include <nx/fusion/model_functions.h>

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Serializable, (json), Serializable_Fields)

} // namespace nx::network::http::test
