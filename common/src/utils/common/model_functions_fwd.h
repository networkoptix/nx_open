#ifndef QN_MODEL_FUNCTIONS_FWD_H
#define QN_MODEL_FUNCTIONS_FWD_H

#include <utils/fusion/fusion_fwd.h>
#include <utils/serialization/binary_fwd.h>
#include <utils/serialization/csv_fwd.h>
#include <utils/serialization/data_stream_fwd.h>
#include <utils/serialization/debug_fwd.h>
#include <utils/serialization/enum_fwd.h>
#include <utils/serialization/json_fwd.h>
#include <utils/serialization/lexical_fwd.h>
#include <utils/serialization/sql_fwd.h>
#include <utils/serialization/ubjson_fwd.h>
#include <utils/serialization/xml_fwd.h>

#define QN_FUSION_DECLARE_FUNCTIONS_hash(TYPE, ... /* PREFIX */)                \
__VA_ARGS__ uint qHash(const TYPE &value, uint seed);

#define QN_FUSION_DECLARE_FUNCTIONS_eq(TYPE, ... /* PREFIX */)                  \
__VA_ARGS__ bool operator==(const TYPE &l, const TYPE &r);                      \
__VA_ARGS__ bool operator!=(const TYPE &l, const TYPE &r);

#define QN_FUSION_DECLARE_FUNCTIONS_metatype(TYPE, ... /* PREFIX */)            \
Q_DECLARE_METATYPE(TYPE)

#endif // QN_MODEL_FUNCTIONS_FWD_H
