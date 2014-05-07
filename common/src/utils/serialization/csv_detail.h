#ifndef QN_SERIALIZATION_CSV_DETAIL_H
#define QN_SERIALIZATION_CSV_DETAIL_H

#include <utility> /* For std::declval. */
#include <type_traits> /* For std::integral_constant. */

#ifndef Q_MOC_RUN
#include <boost/mpl/if.hpp>
#endif // Q_MOC_RUN

#include "csv_fwd.h"


namespace QnCsvDetail {

    template<class T, class Output>
    void serialize_field_internal(const T &value, QnCsvStreamWriter<Output> *stream) {
        serialize_field(value, adl_wrap(stream)); /* That's where ADL kicks in. */
    }

    template<class T, class Output>
    void serialize_record_internal(const T &value, QnCsvStreamWriter<Output> *stream) {
        serialize_record(value, adl_wrap(stream)); /* That's where ADL kicks in. */
    }

    template<class T, class Output>
    void serialize_record_header_internal(const T &value, const QString &prefix, QnCsvStreamWriter<Output> *stream) {
        serialize_record_header(value, prefix, adl_wrap(stream)); /* That's where ADL kicks in. */
    }

    template<class T, class Output>
    void serialize_document_internal(const T &value, QnCsvStreamWriter<Output> *stream) {
        serialize_document(value, adl_wrap(stream)); /* That's where ADL kicks in. */
    }


    // TODO: #Elric #EC2 remove / move
    template<class T>
    struct identity {
        typedef T type;
    };

    struct yes_type { char dummy; };
    struct no_type { char dummy[64]; };


    template<class T0, class T1>
    yes_type has_serialize_field_test(const T0 &, const T1 &, const decltype(serialize_field(std::declval<T0>(), std::declval<T1>())) * = NULL);
    no_type has_serialize_field_test(...);

    template<class T0, class T1>
    yes_type has_serialize_record_test(const T0 &, const T1 &, const decltype(serialize_record(std::declval<T0>(), std::declval<T1>())) * = NULL);
    no_type has_serialize_record_test(...);

    template<class T0, class T1>
    yes_type has_serialize_document_test(const T0 &, const T1 &, const decltype(serialize_document(std::declval<T0>(), std::declval<T1>())) * = NULL);
    no_type has_serialize_document_test(...);

    template<class T, class Output>
    struct is_field: 
        std::integral_constant<bool, sizeof(yes_type) == sizeof(has_serialize_field_test(std::declval<T>(), std::declval<QnCsvStreamWriter<Output> *>()))>
    {};

    template<class T, class Output>
    struct is_record: 
        std::integral_constant<bool, sizeof(yes_type) == sizeof(has_serialize_record_test(std::declval<T>(), std::declval<QnCsvStreamWriter<Output> *>()))>
    {};

    template<class T, class Output>
    struct is_document: 
        std::integral_constant<bool, sizeof(yes_type) == sizeof(has_serialize_document_test(std::declval<T>(), std::declval<QnCsvStreamWriter<Output> *>()))>
    {};


    template<class T, class Output>
    struct csv_category:
        boost::mpl::if_<
            is_document<T, Output>,
            identity<identity<QnCsv::document_tag> >,
            boost::mpl::if_<
                is_record<T, Output>,
                identity<QnCsv::record_tag>,
                boost::mpl::if_<
                    is_field<T, Output>,
                    QnCsv::field_tag,
                    void
                >
            >
        >::type::type
    {};

} // namespace QnCsvDetail

#endif // QN_SERIALIZATION_CSV_DETAIL_H
