#ifndef QN_FUSION_ADAPTOR_H
#define QN_FUSION_ADAPTOR_H

#include <utility>      /* For std::forward. */
#include <type_traits>  /* For std::integral_constant, std::declval. */

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/variadic/elem.hpp>
#include <boost/preprocessor/tuple/size.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/stringize.hpp>
#endif // Q_MOC_RUN

#include "fusion.h"

/**
 * Main API entry point for fusion adaptors.
 *
 * Registers a fusion adaptor for the given class.
 * 
 * \param CLASS                         Class to register adaptor for.
 * \param MEMBER_SEQ                    Preprocessor sequence of member parameter
 *                                      sequences for the given class.
 * \param GLOBAL_SEQ                    Global parameters sequence that will
 *                                      be appended to each one of individual
 *                                      member parameter sequences.
 */
#define QN_FUSION_ADAPT_CLASS(CLASS, MEMBER_SEQ, ... /* GLOBAL_SEQ */)          \
    QN_FUSION_ADAPT_CLASS_I(                                                    \
        CLASS,                                                                  \
        QN_FUSION_EXTEND_MEMBER_SEQ(MEMBER_SEQ),                                \
        QN_FUSION_EXTEND_PROPERTY_SEQ(                                          \
            (object_declval, std::declval<CLASS>())                             \
            (classname, BOOST_PP_STRINGIZE(CLASS))                              \
            (member_count, (std::integral_constant<int, BOOST_PP_SEQ_SIZE(MEMBER_SEQ)>())) \
            __VA_ARGS__                                                         \
        )                                                                       \
    )

#define QN_FUSION_ADAPT_CLASS_I(CLASS, MEMBER_SEQ, GLOBAL_SEQ)                  \
template<class T>                                                               \
struct QnFusionBinding;                                                         \
                                                                                \
template<>                                                                      \
struct QnFusionBinding<CLASS> {                                                 \
    template<int n, int dummy = 0>                                              \
    struct at_c;                                                                \
                                                                                \
    template<class T>                                                           \
    struct at:                                                                  \
        at_c<T::value>                                                          \
    {};                                                                         \
                                                                                \
    struct size {                                                               \
        enum { value = BOOST_PP_SEQ_SIZE(MEMBER_SEQ) };                         \
    };                                                                          \
                                                                                \
    BOOST_PP_SEQ_FOR_EACH_I(QN_FUSION_ADAPT_CLASS_OBJECT_STEP_I, GLOBAL_SEQ, MEMBER_SEQ) \
                                                                                \
    template<class T, class Visitor>                                            \
    static bool visit_members(T &&value, Visitor &&visitor) {                   \
        if(!QnFusionDetail::safe_operator_call(std::forward<Visitor>(visitor), std::forward<T>(value), QnFusion::AccessAdaptor<typename at_c<0>::type>(), QnFusion::start_tag())) \
            return false;                                                       \
        BOOST_PP_REPEAT(BOOST_PP_SEQ_SIZE(MEMBER_SEQ), QN_FUSION_ADAPT_CLASS_FUNCTION_STEP_I, ~) \
        if(!QnFusionDetail::safe_operator_call(std::forward<Visitor>(visitor), std::forward<T>(value), QnFusion::AccessAdaptor<typename at_c<0>::type>(), QnFusion::end_tag())) \
            return false;                                                       \
        return true;                                                            \
    }                                                                           \
};                                                                              \
                                                                                \
template<class Visitor>                                                         \
bool visit_members(const CLASS &value, Visitor &&visitor) {                     \
    return QnFusionBinding<CLASS>::visit_members(value, std::forward<Visitor>(visitor)); \
}                                                                               \
                                                                                \
template<class Visitor>                                                         \
bool visit_members(CLASS &value, Visitor &&visitor) {                           \
    return QnFusionBinding<CLASS>::visit_members(value, std::forward<Visitor>(visitor)); \
}


#define QN_FUSION_ADAPT_CLASS_OBJECT_STEP_I(R, GLOBAL_SEQ, INDEX, PROPERTY_SEQ) \
    QN_FUSION_ADAPT_CLASS_OBJECT_STEP_II(INDEX, GLOBAL_SEQ PROPERTY_SEQ)

#define QN_FUSION_ADAPT_CLASS_OBJECT_STEP_II(INDEX, PROPERTY_SEQ)               \
    template<int dummy>                                                         \
    struct at_c<INDEX, dummy> {                                                 \
        struct type {                                                           \
            typedef type access_type;                                           \
                                                                                \
            template<class Key, class Default>                                  \
            struct at {                                                         \
                typedef Default type;                                           \
            };                                                                  \
                                                                                \
            struct size {                                                       \
                enum { value = BOOST_PP_VARIADIC_SEQ_SIZE(PROPERTY_SEQ) };      \
            };                                                                  \
                                                                                \
            BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_I, ~, PROPERTY_SEQ (member_index, (std::integral_constant<int, INDEX>()))) \
        };                                                                      \
    };

#define QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_I(R, DATA, PROPERTY_TUPLE)       \
    QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_II PROPERTY_TUPLE

#define QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_II(KEY, VALUE)                   \
    template<class Default>                                                     \
    struct at<QN_FUSION_KEY_TYPE(KEY), Default> {                               \
        struct type {                                                           \
            typedef QN_FUSION_PROPERTY_TYPE(KEY, VALUE) result_type;            \
                                                                                \
            result_type operator()() const {                                    \
                return VALUE;                                                   \
            }                                                                   \
        };                                                                      \
    };


// TODO: 
// #Elric instead of having visit_members function, expose only a function
// that returns fusion adaptor for a type. Then implement a processor that
// uses this adaptor. Then implement visit_members via that processor 
// (and without macros).
#define QN_FUSION_ADAPT_CLASS_FUNCTION_STEP_I(Z, INDEX, DATA)                   \
    if(!QnFusionDetail::dispatch_visit(std::forward<Visitor>(visitor), std::forward<T>(value), QnFusion::AccessAdaptor<at_c<INDEX>::type>())) \
        return false;


/**
 * 
 */
#define QN_FUSION_ADAPT_CLASS_SHORT(CLASS, KEYS_TUPLE, MEMBER_SEQ, ... /* GLOBAL_SEQ */) \
    QN_FUSION_ADAPT_CLASS(CLASS, QN_FUSION_UNROLL_SHORTCUT_SEQ(KEYS_TUPLE, MEMBER_SEQ), ##__VA_ARGS__)

/**
 * 
 */
#define QN_FUSION_ADAPT_CLASS_GSN(CLASS, MEMBER_SEQ, ... /* GLOBAL_SEQ */)      \
    QN_FUSION_ADAPT_CLASS_SHORT(CLASS, (getter, setter, name), MEMBER_SEQ, ##__VA_ARGS__)

/**
 * 
 */
#define QN_FUSION_ADAPT_CLASS_GSNC(CLASS, MEMBER_SEQ, ... /* GLOBAL_SEQ */)     \
    QN_FUSION_ADAPT_CLASS_SHORT(CLASS, (getter, setter, name, checker), MEMBER_SEQ, ##__VA_ARGS__)

/**
 *
 */
#define QN_FUSION_ADAPT_STRUCT(STRUCT, FIELD_SEQ, ... /* GLOBAL_SEQ */)         \
    QN_FUSION_ADAPT_CLASS(STRUCT, QN_FUSION_FIELD_SEQ_TO_MEMBER_SEQ(STRUCT, FIELD_SEQ), ##__VA_ARGS__)

/**
 * 
 */
#define QN_FUSION_ADAPT_CLASS_FUNCTIONS(CLASS, FUNCTION_SEQ, MEMBER_SEQ, ... /* GLOBAL_SEQ, PREFIX */) \
    QN_FUSION_ADAPT_CLASS(CLASS, MEMBER_SEQ, BOOST_PP_VARIADIC_ELEM(0, ##__VA_ARGS__,,)) \
    QN_FUSION_DEFINE_FUNCTIONS(CLASS, FUNCTION_SEQ, BOOST_PP_VARIADIC_ELEM(1, ##__VA_ARGS__,,))

/**
 * 
 */
#define QN_FUSION_ADAPT_CLASS_GSN_FUNCTIONS(CLASS, FUNCTION_SEQ, MEMBER_SEQ, ... /* GLOBAL_SEQ, PREFIX */) \
    QN_FUSION_ADAPT_CLASS_GSN(CLASS, MEMBER_SEQ, BOOST_PP_VARIADIC_ELEM(0, ##__VA_ARGS__,,)) \
    QN_FUSION_DEFINE_FUNCTIONS(CLASS, FUNCTION_SEQ, BOOST_PP_VARIADIC_ELEM(1, ##__VA_ARGS__,,))

/**
 * 
 */
#define QN_FUSION_ADAPT_STRUCT_FUNCTIONS(STRUCT, FUNCTION_SEQ, FIELD_SEQ, ... /* GLOBAL_SEQ, PREFIX */) \
    QN_FUSION_ADAPT_STRUCT(STRUCT, FIELD_SEQ, BOOST_PP_VARIADIC_ELEM(0, ##__VA_ARGS__,,)) \
    QN_FUSION_DEFINE_FUNCTIONS(STRUCT, FUNCTION_SEQ, BOOST_PP_VARIADIC_ELEM(1, ##__VA_ARGS__,,))


/**
 * \internal
 * 
 * Converts a field sequence into a standard member sequence.
 * 
 * \param STRUCT                        Name of the struct.
 * \param FIELD_SEQ                     Preprocessor sequence of field names.
 */
#define QN_FUSION_FIELD_SEQ_TO_MEMBER_SEQ(STRUCT, FIELD_SEQ)                    \
    BOOST_PP_SEQ_FOR_EACH(QN_FUSION_FIELD_SEQ_TO_MEMBER_SEQ_STEP_I, STRUCT, FIELD_SEQ)

#define QN_FUSION_FIELD_SEQ_TO_MEMBER_SEQ_STEP_I(R, STRUCT, FIELD)              \
    ((getter, &STRUCT::FIELD)(setter, &STRUCT::FIELD)(name, BOOST_PP_STRINGIZE(FIELD)))


/**
 * \internal
 * 
 * Unrolls a member sequence that uses key shortcuts into its standard form.
 * 
 * For example, the following invocation:
 * 
 * \code
 * QN_FUSION_UNROLL_SHORTCUT_SEQ(
 *     (getter, setter, name),
 *     ((&QSize::width, &QSize::setWidth, "width"))
 *     ((&QSize::height, &QSize::setHeight, "height")(optional, true))
 * )
 * \endcode
 * 
 * Will expand into:
 *
 * \code
 * ((getter, &QSize::width)(setter, &QSize::setWidth)(name, "width"))
 * ((getter, &QSize::height)(setter, &QSize::setHeight)(name, "height")(optional, true))
 * \endcode
 */
#define QN_FUSION_UNROLL_SHORTCUT_SEQ(KEYS_TUPLE, MEMBER_SEQ)                   \
    BOOST_PP_SEQ_FOR_EACH(QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_I, KEYS_TUPLE, MEMBER_SEQ)

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_I(S, KEYS_TUPLE, PROPERTY_SEQ)       \
    (QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_II(KEYS_TUPLE, (BOOST_PP_VARIADIC_SEQ_HEAD(PROPERTY_SEQ))) BOOST_PP_VARIADIC_SEQ_TAIL(PROPERTY_SEQ))

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_II(KEYS_TUPLE, VALUES_TUPLE)         \
    BOOST_PP_REPEAT(BOOST_PP_TUPLE_SIZE(KEYS_TUPLE), QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_I, (KEYS_TUPLE, VALUES_TUPLE))

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_I(Z, INDEX, DATA)               \
    QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_II(BOOST_PP_TUPLE_ELEM(0, DATA), BOOST_PP_TUPLE_ELEM(1, DATA), INDEX)

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_II(KEYS_TUPLE, VALUES_TUPLE, INDEX) \
    (BOOST_PP_TUPLE_ELEM(INDEX, KEYS_TUPLE), BOOST_PP_TUPLE_ELEM(INDEX, VALUES_TUPLE))


/**
 * \internal
 * 
 * Applies property extensions to the given member sequence, returning a new
 * member sequence.
 * 
 * \param MEMBER_SEQ
 */
#define QN_FUSION_EXTEND_MEMBER_SEQ(MEMBER_SEQ)                                 \
    BOOST_PP_SEQ_FOR_EACH(QN_FUSION_EXTEND_MEMBER_SEQ_STEP_I, ~, MEMBER_SEQ)

#define QN_FUSION_EXTEND_MEMBER_SEQ_STEP_I(R, DATA, PROPERTY_SEQ)               \
    (QN_FUSION_EXTEND_PROPERTY_SEQ(PROPERTY_SEQ))


/**
 * \internal
 * 
 * Applies property extensions to the given property sequence, returning a new
 * property sequence.
 * 
 * \param PROPERTY_SEQ
 */
#define QN_FUSION_EXTEND_PROPERTY_SEQ(PROPERTY_SEQ)                             \
    BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_FUSION_EXTEND_PROPERTY_SEQ_STEP_I, ~, PROPERTY_SEQ)

#define QN_FUSION_EXTEND_PROPERTY_SEQ_STEP_I(R, DATA, PROPERTY_TUPLE)           \
    QN_FUSION_PROPERTY_EXTENSION PROPERTY_TUPLE


/**
 * \internal
 *
 * This macro returns a type expression that should be used in return type of a
 * property getter function for the specified key. By default it simply 
 * returns <tt>decltype(VALUE)</tt>.
 * 
 * To override the default return value for some key <tt>some_key</tt>, use
 * the following code:
 * \code
 * #define QN_FUSION_PROPERTY_IS_TYPED_FOR_some_key true
 * #define QN_FUSION_PROPERTY_TYPE_FOR_some_key QString // Or your custom type
 * \endcode
 * 
 * This might be useful when <tt>decltype</tt> cannot be used because
 * <tt>VALUE</tt> might be a lambda. This is also a way of saving on 
 * <tt>decltype</tt> calls when property type is known at key definition time.
 *
 * \param KEY                           Fusion key.
 * \param VALUE                         Value specified by the user for this key.
 * \returns                             Type expression that should be used for
 *                                      return type of a property getter function.
 */
#define QN_FUSION_PROPERTY_TYPE(KEY, VALUE)                                     \
    BOOST_PP_OVERLOAD(                                                          \
        QN_FUSION_PROPERTY_TYPE_I_,                                             \
        ~ BOOST_PP_CAT(QN_FUSION_PROPERTY_IS_TYPED_CHECK_, BOOST_PP_CAT(QN_FUSION_PROPERTY_IS_TYPED_FOR_, KEY)) ~ \
    )(KEY, VALUE)
 
#define QN_FUSION_PROPERTY_IS_TYPED_CHECK_true ,

#define QN_FUSION_PROPERTY_TYPE_I_1(KEY, VALUE) decltype(VALUE)
#define QN_FUSION_PROPERTY_TYPE_I_2(KEY, VALUE) BOOST_PP_CAT(QN_FUSION_PROPERTY_TYPE_FOR_, KEY)


/**
 * \internal
 * 
 * This macro returns property extension for the provided property pair, 
 * which essentially is a sequence of property pairs that should be used instead
 * of the original one. By default it simply returns <tt>(KEY, VALUE)</tt>.
 * 
 * To specify a property extension for some key <tt>some_key</tt>, use
 * the following code:
 * \code
 * #define QN_FUSION_PROPERTY_IS_EXTENDED_FOR_some_key true
 * #define QN_FUSION_PROPERTY_EXTENSION_FOR_some_key(KEY, VALUE) (KEY, VALUE)(BOOST_PP_CAT(qt_, KEY), QLatin1Literal(VALUE)) // Or your custom extension
 * \endcode
 * 
 * \param KEY                           Fusion key.
 * \param VALUE                         Value specified by the user for this key.
 * \returns                             Property extension.
 */
#define QN_FUSION_PROPERTY_EXTENSION(KEY, VALUE)                                \
    BOOST_PP_OVERLOAD(                                                          \
        QN_FUSION_PROPERTY_EXTENSION_I_,                                        \
        ~ BOOST_PP_CAT(QN_FUSION_PROPERTY_IS_EXTENDED_CHECK_, BOOST_PP_CAT(QN_FUSION_PROPERTY_IS_EXTENDED_FOR_, KEY)) ~ \
    )(KEY, VALUE)

#define QN_FUSION_PROPERTY_IS_EXTENDED_CHECK_true ,

#define QN_FUSION_PROPERTY_EXTENSION_I_1(KEY, VALUE) (KEY, VALUE)
#define QN_FUSION_PROPERTY_EXTENSION_I_2(KEY, VALUE) BOOST_PP_CAT(QN_FUSION_PROPERTY_EXTENSION_FOR_, KEY)(KEY, VALUE)



#endif // QN_FUSION_ADAPTOR_H