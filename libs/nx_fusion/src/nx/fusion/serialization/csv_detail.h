// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_SERIALIZATION_CSV_DETAIL_H
#define QN_SERIALIZATION_CSV_DETAIL_H

#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

#include "csv_fwd.h"

namespace QnCsvDetail {

    template<class Output>
    void serialize_header(const QString &prefix, QnCsvStreamWriter<Output> *stream, const void *) {
        /* Default implementation for field types. */
        stream->writeField(prefix + QStringLiteral("value"));
    }

    template<class T, class Output>
    void serialize_header_internal(const QString &prefix, QnCsvStreamWriter<Output> *stream, const T *dummy) {
        serialize_header(prefix, disable_user_conversions(stream), dummy); /* That's where ADL kicks in. */
    }

    template<class T, class Output>
    struct type_category_automatic:
            std::conditional_t<
                requires(T t, QnCsvStreamWriter<Output>* out){serialize_header(t, out); },

                std::type_identity<QnCsv::record_tag>,

                std::conditional<
                    requires(T t, QnCsvStreamWriter<Output>* out){serialize(t, out); },
                    QnCsv::field_tag,
                    void
                >
            >
    {};

    template<std::ranges::range T, typename Output>
    struct type_category_automatic<T, Output>:
        std::type_identity<QnCsv::document_tag>
    {};

    template<typename T, typename Output>
    struct type_category_automatic<std::optional<T>, Output>:
        type_category_automatic<T, Output>
    {};

    template<class T>
    struct type_category_user_defined {
        typedef decltype(csv_type_category(std::declval<const T*>())) type;
    };

    template<class T>
    struct type_category_user_defined<std::optional<T>>:
        type_category_user_defined<T>
    {};

    template<class T, class Output>
    struct type_category:
        std::conditional_t<
            requires(const T* t){csv_type_category(t); },
            type_category_user_defined<T>,
            type_category_automatic<T, Output>
        >
    {};

    template <typename T, typename Output>
    struct type_category<std::optional<T>, Output>:
        type_category<T, Output>
    {};

} // namespace QnCsvDetail

#endif // QN_SERIALIZATION_CSV_DETAIL_H
