#ifndef QN_MODEL_FUNCTIONS_FWD_H
#define QN_MODEL_FUNCTIONS_FWD_H

#include <QtCore/QMetaType>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/serialization/binary_fwd.h>
#include <nx/fusion/serialization/csv_fwd.h>
#include <nx/fusion/serialization/data_stream_fwd.h>
#include <nx/fusion/serialization/debug_fwd.h>
#include <nx/fusion/serialization/enum_fwd.h>
#include <nx/fusion/serialization/json_fwd.h>
#include <nx/fusion/serialization/lexical_fwd.h>
#include <nx/fusion/serialization/sql_fwd.h>
#include <nx/fusion/serialization/ubjson_fwd.h>
#include <nx/fusion/serialization/xml_fwd.h>
#include <nx/fusion/serialization/compressed_time_fwd.h>

#define QN_FUSION_DECLARE_FUNCTIONS_hash(TYPE, ... /* PREFIX */)                \
__VA_ARGS__ uint qHash(const TYPE &value, uint seed);

#define QN_FUSION_DECLARE_FUNCTIONS_eq(TYPE, ... /* PREFIX */)                  \
__VA_ARGS__ bool operator==(const TYPE &l, const TYPE &r);                      \
__VA_ARGS__ bool operator!=(const TYPE &l, const TYPE &r);

#define QN_FUSION_DECLARE_FUNCTIONS_metatype(TYPE, ... /* PREFIX */)            \
Q_DECLARE_METATYPE(TYPE)

/**
* Same as <tt>Q_GADGET</tt>, but doesn't trigger MOC, and can be used in namespaces.
* The only sane use case is for generating metainformation for enums in
* namespaces (e.g. for use with <tt>QnEnumNameMapper</tt>).
*/
#ifdef Q_MOC_RUN
#define QN_DECLARE_METAOBJECT_HEADER(TYPE, ENUMS, FLAGS)    \
class TYPE                                                  \
{                                                           \
    Q_GADGET                                                \
    Q_ENUMS(ENUMS)                                          \
    Q_FLAGS(FLAGS)                                          \
public:
#else
#define QN_DECLARE_METAOBJECT_HEADER(TYPE, ENUMS, FLAGS)    \
    namespace TYPE                                          \
{                                                           \
    extern const QMetaObject staticMetaObject;              \


#endif


#endif // QN_MODEL_FUNCTIONS_FWD_H
