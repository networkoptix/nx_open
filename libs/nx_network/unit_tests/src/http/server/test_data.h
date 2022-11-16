// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/http_types.h>
#include <nx/reflect/instrument.h>

namespace nx::network::http::test {

struct Serializable
{
    int dummyInt = 0;

    bool operator==(const Serializable& right) const
    {
        return dummyInt == right.dummyInt;
    }
};

bool serializeToHeaders(nx::network::http::HttpHeaders* where, const Serializable& what);
bool deserializeFromHeaders(const nx::network::http::HttpHeaders& from, Serializable* what);

NX_REFLECTION_INSTRUMENT(Serializable, (dummyInt));

} // namespace nx::network::http::test
