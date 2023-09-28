// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_SERIALIZATION_CSV_DETAIL_H
#define QN_SERIALIZATION_CSV_DETAIL_H

#include <optional>
#include <type_traits> /* For std::integral_constant. */
#include <utility> /* For std::declval. */

#ifndef Q_MOC_RUN
    #include <boost/mpl/if.hpp>
    #include <boost/mpl/or.hpp>
    #include <boost/range/has_range_iterator.hpp>
#endif // Q_MOC_RUN

#include <nx/utils/type_traits.h>

#include "csv_fwd.h"

namespace QnCsvDetail {
    using namespace QnTypeTraits;

    template<class Output>
    void serialize_header(const QString &prefix, QnCsvStreamWriter<Output> *stream, const void *) {
        /* Default implementation for field types. */
        stream->writeField(prefix + QStringLiteral("value"));
    }

    template<class T, class Output>
    void serialize_header_internal(const QString &prefix, QnCsvStreamWriter<Output> *stream, const T *dummy) {
        serialize_header(prefix, disable_user_conversions(stream), dummy); /* That's where ADL kicks in. */
    }


    template<class T0, class T1>
    yes_type has_serialize_test(const T0 &, const T1 &, const decltype(serialize(std::declval<T0>(), std::declval<T1>())) * = NULL);
    no_type has_serialize_test(...);

    template<class T0, class T1>
    yes_type has_serialize_header_test(const T0 &, const T1 &, const decltype(serialize_header(std::declval<T0>(), std::declval<T1>())) * = NULL);
    no_type has_serialize_header_test(...);

    template<class T0>
    yes_type has_csv_type_category_test(const T0 &, const decltype(csv_type_category(std::declval<T0>())) * = NULL);
    no_type has_csv_type_category_test(...);

    template<class T, class Output>
    struct has_serialize:
        std::integral_constant<bool, sizeof(yes_type) == sizeof(has_serialize_test(std::declval<T>(), std::declval<QnCsvStreamWriter<Output> *>()))>
    {};

    template<class T, class Output>
    struct has_serialize_header:
        std::integral_constant<bool, sizeof(yes_type) == sizeof(has_serialize_header_test(std::declval<T>(), std::declval<QnCsvStreamWriter<Output> *>()))>
    {};

    template<class T>
    struct has_csv_type_category:
        std::integral_constant<bool, sizeof(yes_type) == sizeof(has_csv_type_category_test(std::declval<const T *>()))>
    {};

    template<typename T>
    struct UnwrapOptional
    {
        using type = T;
    };

    template<typename T>
    struct UnwrapOptional<std::optional<T>>
    {
        using type = T;
    };

    template<typename T>
    using UnwrapOptionalType = typename UnwrapOptional<T>::type;

    template<class T, class Output>
    struct type_category_automatic:
        boost::mpl::if_<
            boost::mpl::or_<
                boost::has_range_const_iterator<UnwrapOptionalType<T>>,
                boost::has_range_iterator<UnwrapOptionalType<T>>>,
            identity<identity<QnCsv::document_tag> >,
            boost::mpl::if_<
                has_serialize_header<UnwrapOptionalType<T>, Output>,
                identity<QnCsv::record_tag>,
                boost::mpl::if_<
                    has_serialize<UnwrapOptionalType<T>, Output>,
                    QnCsv::field_tag,
                    void
                >
            >
        >::type::type
    {};

    template<class T>
    struct type_category_user_defined {
        typedef decltype(csv_type_category(std::declval<const UnwrapOptionalType<T>*>())) type;
    };

    template<class T, class Output>
    struct type_category:
        boost::mpl::if_<
            has_csv_type_category<UnwrapOptionalType<T>>,
            type_category_user_defined<UnwrapOptionalType<T>>,
            type_category_automatic<UnwrapOptionalType<T>, Output>
        >::type
    {};

} // namespace QnCsvDetail

#endif // QN_SERIALIZATION_CSV_DETAIL_H
