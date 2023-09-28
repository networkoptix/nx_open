// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/serialization/csv_fwd.h>
#include <nx/fusion/serialization/debug_fwd.h>
#include <nx/fusion/serialization/json_fwd.h>
#include <nx/fusion/serialization/lexical_fwd.h>
#include <nx/fusion/serialization/sql_fwd.h>
#include <nx/fusion/serialization/ubjson_fwd.h>
#include <nx/fusion/serialization/xml_fwd.h>

#define QN_FUSION_DECLARE_FUNCTIONS_hash(TYPE, ... /* PREFIX */)                \
__VA_ARGS__ size_t qHash(const TYPE &value, size_t seed = 0);

#define QN_FUSION_DECLARE_FUNCTIONS_eq(TYPE, ... /* PREFIX */)                  \
__VA_ARGS__ bool operator==(const TYPE &l, const TYPE &r);                      \
__VA_ARGS__ bool operator!=(const TYPE &l, const TYPE &r);
