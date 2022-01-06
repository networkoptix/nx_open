// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_MODEL_FUNCTIONS_FWD_H
#define QN_MODEL_FUNCTIONS_FWD_H

#include <QtCore/QMetaType>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/serialization/csv_fwd.h>
#include <nx/fusion/serialization/data_stream_fwd.h>
#include <nx/fusion/serialization/debug_fwd.h>
#include <nx/fusion/serialization/json_fwd.h>
#include <nx/fusion/serialization/lexical_fwd.h>
#include <nx/fusion/serialization/sql_fwd.h>
#include <nx/fusion/serialization/ubjson_fwd.h>
#include <nx/fusion/serialization/xml_fwd.h>
#include <nx/fusion/serialization/compressed_time_fwd.h>

#define QN_FUSION_DECLARE_FUNCTIONS_hash(TYPE, ... /* PREFIX */)                \
__VA_ARGS__ size_t qHash(const TYPE &value, size_t seed = 0);

#define QN_FUSION_DECLARE_FUNCTIONS_eq(TYPE, ... /* PREFIX */)                  \
__VA_ARGS__ bool operator==(const TYPE &l, const TYPE &r);                      \
__VA_ARGS__ bool operator!=(const TYPE &l, const TYPE &r);

#define QN_FUSION_DECLARE_FUNCTIONS_metatype(TYPE, ... /* PREFIX */)            \
Q_DECLARE_METATYPE(TYPE)

#endif // QN_MODEL_FUNCTIONS_FWD_H
