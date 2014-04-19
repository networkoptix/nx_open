#ifndef QN_FUSION_ADAPTORS_H
#define QN_FUSION_ADAPTORS_H

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/variadic/elem.hpp>
#include <boost/preprocessor/tuple/size.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/mpl/integral_c.hpp>
#endif // Q_MOC_RUN

#include "fusion.h"


namespace QnFusion {
    /**
     * \fn invoke
     * 
     * This set of overloaded functions presents an interface for invoking 
     * setters and getters returned by member adaptors.
     */

    template<class Class, class T>
    T invoke(T Class::*getter, const Class &object) {
        return object.*getter;
    }

    template<class Class, class T>
    T invoke(T (Class::*getter)() const, const Class &object) {
        return (object.*getter)();
    }

    template<class Getter, class Class>
    auto invoke(const Getter &getter, const Class &object) -> decltype(getter(object)) {
        return getter(object);
    }

    template<class Class, class T>
    void invoke(T Class::*setter, Class &object, const T &value) {
        object.*setter = value;
    }

    template<class Class, class R, class P, class T>
    void invoke(R (Class::*setter)(P), Class &object, const T &value) {
        (object.*setter)(value);
    }

    template<class Setter, class Class, class T>
    void invoke(const Setter &setter, Class &object, const T &value) {
        setter(object, value);
    }

} // namespace QnFusion

/**
 * 
 */
#define QN_FUSION_ADAPT_CLASS(CLASS, MEMBER_SEQ, ... /* GLOBAL_SEQ */)          \
    QN_FUSION_ADAPT_CLASS_I(CLASS, MEMBER_SEQ, BOOST_PP_VARIADIC_SEQ_NIL __VA_ARGS__)

#define QN_FUSION_ADAPT_CLASS_I(CLASS, MEMBER_SEQ, GLOBAL_SEQ)                  \
template<class T>                                                               \
struct QnFusionBinding;                                                         \
                                                                                \
template<>                                                                      \
struct QnFusionBinding<CLASS> {                                                 \
    BOOST_PP_SEQ_FOR_EACH_I(QN_FUSION_ADAPT_CLASS_OBJECT_STEP_I, GLOBAL_SEQ, MEMBER_SEQ) \
                                                                                \
    template<class T, class Visitor>                                            \
    static bool visit_members(T &&value, Visitor &&visitor) {                   \
        if(!QnFusionDetail::initialize_visitor(std::forward<Visitor>(visitor), std::forward<T>(value))) \
            return false;                                                       \
        BOOST_PP_REPEAT(BOOST_PP_SEQ_SIZE(MEMBER_SEQ), QN_FUSION_ADAPT_CLASS_FUNCTION_STEP_I, ~) \
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
    struct BOOST_PP_CAT(MemberAdaptor, INDEX) {                                 \
        typedef BOOST_PP_CAT(MemberAdaptor, INDEX) this_type;                   \
                                                                                \
        template<class Tag>                                                     \
        struct has_tag:                                                         \
            boost::mpl::identity<boost::mpl::false_>                            \
        {};                                                                     \
                                                                                \
        template<class T>                                                       \
        struct result;                                                          \
                                                                                \
        BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_I, ~, PROPERTY_SEQ (index, INDEX)) \
    };

#define QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_I(R, DATA, PROPERTY_TUPLE)       \
    QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_II PROPERTY_TUPLE

#define QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_II(TAG, VALUE)                   \
    template<>                                                                  \
    struct has_tag<QN_FUSION_TAG_TYPE(TAG)>:                                    \
        boost::mpl::identity<boost::mpl::true_>                                 \
    {};                                                                         \
                                                                                \
    template<class F>                                                           \
    struct result<F(QN_FUSION_TAG_TYPE(TAG))> {                                 \
        typedef QN_FUSION_PROPERTY_TYPE(TAG, VALUE) type;                       \
    };                                                                          \
                                                                                \
    result<this_type(QN_FUSION_TAG_TYPE(TAG))>::type operator()(const QN_FUSION_TAG_TYPE(TAG) &) const { \
        return QN_FUSION_PROPERTY_VALUE(TAG, VALUE);                            \
    }


#define QN_FUSION_ADAPT_CLASS_FUNCTION_STEP_I(Z, INDEX, DATA)                   \
    if(!visitor(std::forward<T>(value), QnFusion::MemberAdaptor<BOOST_PP_CAT(MemberAdaptor, INDEX)>())) \
        return false;


/**
 * 
 */
#define QN_FUSION_ADAPT_CLASS_SHORT(CLASS, TAGS_TUPLE, MEMBER_SEQ, ... /* GLOBAL_SEQ */) \
    QN_FUSION_ADAPT_CLASS(CLASS, QN_FUSION_UNROLL_SHORTCUT_SEQ(TAGS_TUPLE, MEMBER_SEQ), ##__VA_ARGS__)

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
 */
#define QN_FUSION_FIELD_SEQ_TO_MEMBER_SEQ(STRUCT, FIELD_SEQ)                    \
    BOOST_PP_SEQ_FOR_EACH(QN_FUSION_FIELD_SEQ_TO_MEMBER_SEQ_STEP_I, STRUCT, FIELD_SEQ)

#define QN_FUSION_FIELD_SEQ_TO_MEMBER_SEQ_STEP_I(R, STRUCT, FIELD)              \
    ((getter, &STRUCT::FIELD)(setter, &STRUCT::FIELD)(name, BOOST_PP_STRINGIZE(FIELD)))


/**
 * \internal
 * 
 * Unrolls a member sequence that uses tag shortcuts into its standard form.
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
#define QN_FUSION_UNROLL_SHORTCUT_SEQ(TAGS_TUPLE, MEMBER_SEQ)                   \
    BOOST_PP_SEQ_FOR_EACH(QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_I, TAGS_TUPLE, MEMBER_SEQ)

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_I(R, TAGS_TUPLE, PROPERTY_SEQ)       \
    (QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_II(TAGS_TUPLE, (BOOST_PP_VARIADIC_SEQ_HEAD(PROPERTY_SEQ))) BOOST_PP_VARIADIC_SEQ_TAIL(PROPERTY_SEQ))

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_II(TAGS_TUPLE, VALUES_TUPLE)         \
    BOOST_PP_REPEAT(BOOST_PP_TUPLE_SIZE(TAGS_TUPLE), QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_I, (TAGS_TUPLE, VALUES_TUPLE))

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_I(Z, INDEX, DATA)               \
    QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_II(BOOST_PP_TUPLE_ELEM(0, DATA), BOOST_PP_TUPLE_ELEM(1, DATA), INDEX)

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_II(TAGS_TUPLE, VALUES_TUPLE, INDEX) \
    (BOOST_PP_TUPLE_ELEM(INDEX, TAGS_TUPLE), BOOST_PP_TUPLE_ELEM(INDEX, VALUES_TUPLE))


/**
 * \internal
 *
 * This macro returns a type expression that should be used in return type of a
 * property getter function for the specified tag. By default it simply 
 * returns <tt>decltype(VALUE)</tt>.
 * 
 * To override the default return value for some tag <tt>some_tag</tt>, use
 * the following code:
 * \code
 * #define QN_FUSION_PROPERTY_IS_TYPED_FOR_some_tag ,
 * #define QN_FUSION_PROPERTY_TYPE_FOR_some_tag QString // Or your custom type
 * \endcode
 * 
 * This might be useful when <tt>decltype</tt> cannot be used because
 * <tt>VALUE</tt> might be a lambda.
 *
 * \param TAG                           Fusion tag.
 * \param VALUE                         Value specified by the user for this tag.
 * \returns                             Type expression that should be used for
 *                                      return type of a property getter function.
 */
#define QN_FUSION_PROPERTY_TYPE(TAG, VALUE)                                          \
    BOOST_PP_OVERLOAD(QN_FUSION_PROPERTY_TYPE_I_, ~ BOOST_PP_CAT(QN_FUSION_PROPERTY_IS_TYPED_FOR_, TAG) ~)(TAG, VALUE)
    
#define QN_FUSION_PROPERTY_TYPE_I_1(TAG, VALUE) decltype(QN_FUSION_PROPERTY_VALUE(TAG, VALUE))
#define QN_FUSION_PROPERTY_TYPE_I_2(TAG, VALUE) BOOST_PP_CAT(QN_FUSION_PROPERTY_TYPE_FOR_, TAG)


/**
 * \internal
 * 
 * This macro returns an expression that should be used in return statement
 * of a property getter function for the specified tag. By default it simply 
 * returns <tt>VALUE</tt>.
 * 
 * To override the default return value for some tag <tt>some_tag</tt>, use
 * the following code:
 * \code
 * #define QN_FUSION_PROPERTY_IS_WRAPPED_FOR_some_tag ,
 * #define QN_FUSION_PROPERTY_WRAPPER_FOR_some_tag QLatin1Literal // Or your custom wrapper macro
 * \endcode
 * 
 * \param TAG                           Fusion tag.
 * \param VALUE                         Value specified by the user for this tag.
 * \returns                             Expression that should be used in the
 *                                      return statement of a property getter function.
 */
#define QN_FUSION_PROPERTY_VALUE(TAG, VALUE)                                         \
    BOOST_PP_OVERLOAD(QN_FUSION_PROPERTY_VALUE_I_, ~ BOOST_PP_CAT(QN_FUSION_PROPERTY_IS_WRAPPED_FOR_, TAG) ~)(TAG, VALUE)

#define QN_FUSION_PROPERTY_VALUE_I_1(TAG, VALUE) VALUE
#define QN_FUSION_PROPERTY_VALUE_I_2(TAG, VALUE) BOOST_PP_CAT(QN_FUSION_PROPERTY_WRAPPER_FOR_, TAG)(VALUE)


#endif // QN_FUSION_ADAPTORS_H