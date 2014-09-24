#ifndef QN_SERIALIZATION_ENUM_H
#define QN_SERIALIZATION_ENUM_H

#include <type_traits> /* For std::is_enum, std::true_type. */

#include "enum_fwd.h"


template<class T>
class QFlags;


namespace QnSerializationDetail {
    template<class T>
    void check_enum_binary(const T *, ...) {
        static_assert(sizeof(T) > (1024 * 1024), "Numeric serialization is not enabled for enum T. Did you forget to use QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(T)?");
    }

    template<class T>
    void check_enum_binary_internal(const T *) {
        static_assert(sizeof(T) <= sizeof(int), "Enums larger than int in size are not supported.");

        check_enum_binary(static_cast<const T *>(NULL));
    }

    template<class T>
    void check_enum_binary_internal(const QFlags<T> *) {
        check_enum_binary(static_cast<const T *>(NULL));
    }

} // namespace QnSerializationDetail


namespace QnSerialization {
    template<class T>
    void check_enum_binary() {
        QnSerializationDetail::check_enum_binary_internal(static_cast<const T *>(NULL));
    }

    template<class T>
    struct is_flags:
        std::false_type
    {};

    template<class T>
    struct is_flags<QFlags<T> >:
        std::true_type
    {};


    template<class T>
    struct is_enum:
        std::is_enum<T>
    {};


    template<class T>
    struct is_enum_or_flags:
        std::integral_constant<
            bool, 
            is_flags<T>::value || is_enum<T>::value
        >
    {};

} // namespace QnSerialization


#endif // QN_SERIALIZATION_ENUM_H
