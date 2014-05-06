#ifndef QN_SERIALIZATION_ENUM_H
#define QN_SERIALIZATION_ENUM_H

#include "enum_fwd.h"

namespace QnSerializationDetail {
    template<class T>
    void check_enum_binary(const T *, ...) {
        static_assert(sizeof(T) > (1024 * 1024), "Numeric serialization is not enabled for enum T. Did you forget to use QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(T)?");
    }

    template<class T>
    void check_enum_binary_internal(const T *dummy) {
        check_enum_binary(dummy);
    }
} // namespace QnSerializationDetail


namespace QnSerialization {
    template<class T>
    void check_enum_binary() {
        QnSerializationDetail::check_enum_binary_internal(static_cast<const T *>(NULL));
    }
} // namespace QnSerialization


#endif // QN_SERIALIZATION_ENUM_H
