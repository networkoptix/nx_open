#ifndef QN_SERIALIZATION_FUNCTIONS_H
#define QN_SERIALIZATION_FUNCTIONS_H

#include <utility> /* For std::forward and std::declval. */

#include <utils/preprocessor/variadic_seq_for_each.h>

#include <boost/preprocessor/seq/fold_left.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/comparison/not_equal.hpp>
#include <boost/preprocessor/tuple/size.hpp>

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/sizeof.hpp>
#include <boost/mpl/has_xxx.hpp>
#include <boost/mpl/and.hpp>

namespace QssDetail {
    template<class T, class Visitor>
    bool visit_members_internal(T &&value, Visitor &&visitor) {
        return visit_members(std::forward<T>(value), std::forward<Visitor>(visitor)); /* That's the place where ADL kicks in. */
    }

    struct EmptyVisitor {};

    template<class T>
    boost::type_traits::yes_type has_visit_members_test(const T &, const decltype(visit_members(std::declval<T>(), std::declval<EmptyVisitor>())) * = NULL);
    boost::type_traits::no_type has_visit_members_test(...);

    template<class T>
    struct has_visit_members: 
        boost::mpl::equal_to<
            boost::mpl::sizeof_<boost::type_traits::yes_type>, 
            boost::mpl::size_t<sizeof(has_visit_members_test(std::declval<T>()))>
        > 
    {};

    BOOST_MPL_HAS_XXX_TRAIT_DEF(type)

    template<class Adaptor, class Tag, class Default, bool hasTag = Adaptor::template has_tag<Tag>::value>
    struct DefaultGetter {
        typedef decltype(Adaptor::get(Tag())) result_type;
        result_type operator()() const {
            return Adaptor::get(Tag());
        }
    };

    template<class Adaptor, class Tag, class Default>
    class DefaultGetter<Adaptor, Tag, Default, false> {
        typedef Default result_type;
        result_type operator()() const {
            return Default();
        }
    };

} // namespace QssDetail


namespace Qss {
    /**
     * Main API entry point. Iterates through the members of previously 
     * registered type.
     * 
     * Visitor is expected to define <tt>operator()</tt> of the following form:
     * \code
     * template<class T, class Member>
     * bool operator()(const T &, const Member &);
     * \endcode
     *
     * Here <tt>Member</tt> template parameter presents an interface defined
     * by <tt>AdaptorWrapper</tt> class. Return value of false indicates that
     * iteration should be stopped, and in this case the function will
     * return false.
     *
     * \param value                     Value to iterate through.
     * \param visitor                   Visitor class to apply to members.
     * \see MemberAdaptor
     */
    template<class T, class Visitor>
    bool visit_members(T &&value, Visitor &&visitor) {
        return QssDetail::visit_members_internal(std::forward<T>(value), std::forward<Visitor>(visitor));
    }

    /**
     * This class is the external interface that is to be used by visitor
     * classes.
     */
    template<class Adaptor>
    struct MemberAdaptor {
        template<class Tag>
        static decltype(Adaptor::get(Tag())) 
        get() {
            return Adaptor::get(Tag());
        }

        template<class Tag, class Default>
        static typename QssDetail::DefaultGetter<Adaptor, Tag, Default>::result_type
        get() {
            return QssDetail::DefaultGetter<Adaptor, Tag, Default>()();
        }
    };

    /**
     * \param D                         Data type.
     * \returns                         Serialization visitor type that was
     *                                  registered for the given data type.
     * \see QSS_REGISTER_VISITORS
     */
    template<class D>
    struct serialization_visitor_type {};

    /**
     * \param D                         Data type.
     * \returns                         Deserialization visitor type that was
     *                                  registered for the given data type.
     * \see QSS_REGISTER_VISITORS
     */
    template<class D>
    struct deserialization_visitor_type {};

    template<class D>
    struct has_serialization_visitor_type:
        QssDetail::has_type<serialization_visitor_type<D> >
    {};

    template<class D>
    struct has_deserialization_visitor_type:
        QssDetail::has_type<deserialization_visitor_type<D> >
    {};

    template<class T>
    struct has_visit_members: 
        QssDetail::has_visit_members<T>
    {};

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

} // namespace Qss

/**
 * This macro defines a new Qss tag. It must be used in global namespace.
 * Defined tag can then be accessed from the Qss namespace.
 * 
 * \param TAG                           Tag to define.
 */
#define QSS_DEFINE_TAG(TAG)                                                     \
namespace Qss { struct TAG {}; }

QSS_DEFINE_TAG(index)
QSS_DEFINE_TAG(getter)
QSS_DEFINE_TAG(setter)
QSS_DEFINE_TAG(name)
QSS_DEFINE_TAG(optional)

#define QSS_TAG_IS_TYPED_FOR_index ,
#define QSS_TAG_TYPE_FOR_index int

#define QSS_TAG_IS_TYPED_FOR_name ,
#define QSS_TAG_TYPE_FOR_name QString
#define QSS_TAG_IS_WRAPPED_FOR_name ,
#define QSS_TAG_WRAPPER_FOR_name lit

#define QSS_TAG_IS_TYPED_FOR_optional ,
#define QSS_TAG_TYPE_FOR_optional bool

/**
 * This macro registers Qss visitors that are to be used when serializing
 * and deserializing to/from the given data class. This macro must be used 
 * in global namespace.
 * 
 * Defining the visitors makes all Qss adapted classes instantly (de)serializable
 * to/from the given data class.
 * 
 * \param DATA_CLASS                    Data class to define visitors for.
 * \param SERIALIZATION_VISITOR         Serialization visitor class.
 * \param DESERIALIZATION_VISITOR       Deserialization visitor class.
 */
#define QSS_REGISTER_VISITORS(DATA_CLASS, SERIALIZATION_VISITOR, DESERIALIZATION_VISITOR) \
    QSS_REGISTER_SERIALIZATION_VISITOR(DATA_CLASS, SERIALIZATION_VISITOR)       \
    QSS_REGISTER_DESERIALIZATION_VISITOR(DATA_CLASS, DESERIALIZATION_VISITOR)

/**
 * \see QSS_REGISTER_VISITORS
 */
#define QSS_REGISTER_SERIALIZATION_VISITOR(DATA_CLASS, SERIALIZATION_VISITOR)   \
namespace Qss {                                                                 \
    template<>                                                                  \
    struct serialization_visitor_type<DATA_CLASS>:                              \
        boost::mpl::identity<SERIALIZATION_VISITOR>                             \
    {};                                                                         \
}

/**
 * \see QSS_REGISTER_VISITORS
 */
#define QSS_REGISTER_DESERIALIZATION_VISITOR(DATA_CLASS, DESERIALIZATION_VISITOR) \
namespace Qss {                                                                 \
    template<>                                                                  \
    struct deserialization_visitor_type<DATA_CLASS>:                            \
        boost::mpl::identity<DESERIALIZATION_VISITOR>                           \
    {};                                                                         \
}


/**
 * 
 */
#define QSS_DEFINE_CLASS_ADAPTOR(CLASS, MEMBER_SEQ, ... /* GLOBAL_SEQ */)       \
    QSS_DEFINE_CLASS_ADAPTOR_I(CLASS, QSS_ENUM_SEQ(MEMBER_SEQ), BOOST_PP_SEQ_NIL __VA_ARGS__)

#define QSS_DEFINE_CLASS_ADAPTOR_I(CLASS, MEMBER_SEQ, GLOBAL_SEQ)               \
template<class T>                                                               \
struct QssBinding;                                                              \
                                                                                \
template<>                                                                      \
struct QssBinding<CLASS> {                                                      \
    BOOST_PP_SEQ_FOR_EACH(QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_I, GLOBAL_SEQ, MEMBER_SEQ) \
                                                                                \
    template<class T, class Visitor>                                            \
    static bool visit_members(T &&value, Visitor &&visitor) {                   \
        BOOST_PP_SEQ_FOR_EACH(QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_I, ~, MEMBER_SEQ) \
        return true;                                                            \
    }                                                                           \
};                                                                              \
                                                                                \
template<class Visitor>                                                         \
bool visit_members(const CLASS &value, Visitor &&visitor) {                     \
    return QssBinding<CLASS>::visit_members(value, std::forward<Visitor>(visitor)); \
}                                                                               \
                                                                                \
template<class Visitor>                                                         \
bool visit_members(CLASS &value, Visitor &&visitor) {                           \
    return QssBinding<CLASS>::visit_members(value, std::forward<Visitor>(visitor)); \
}


#define QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_I(R, GLOBAL_SEQ, PROPERTY_TUPLE)   \
    QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_II(                                    \
        BOOST_PP_TUPLE_ELEM(0, PROPERTY_TUPLE),                                 \
        GLOBAL_SEQ BOOST_PP_TUPLE_ELEM(1, PROPERTY_TUPLE)                       \
    )

#define QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_II(INDEX, PROPERTY_SEQ)            \
    struct BOOST_PP_CAT(MemberAdaptor, INDEX) {                                 \
        template<class Tag>                                                     \
        struct has_tag:                                                         \
            boost::mpl::false_type                                              \
        {};                                                                     \
                                                                                \
        BOOST_PP_VARIADIC_SEQ_FOR_EACH(QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_STEP_I, ~, PROPERTY_SEQ (index, INDEX)) \
    };

#define QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_STEP_I(R, DATA, PROPERTY_TUPLE)    \
    QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_STEP_II PROPERTY_TUPLE

#define QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_STEP_II(TAG, VALUE)                \
    template<>                                                                  \
    struct has_tag<Qss::TAG>:                                                   \
        boost::mpl::true_type                                                   \
    {};                                                                         \
                                                                                \
    static QSS_TAG_TYPE(TAG, VALUE) get(const Qss::TAG &) {                     \
        return QSS_TAG_VALUE(TAG, VALUE);                                       \
    }


#define QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_I(R, DATA, PROPERTY_TUPLE)       \
    QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_II(                                  \
        BOOST_PP_TUPLE_ELEM(0, PROPERTY_TUPLE)                                  \
    )

#define QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_II(INDEX)                        \
    if(!visitor(std::forward<T>(value), Qss::MemberAdaptor<BOOST_PP_CAT(MemberAdaptor, INDEX)>())) \
        return false;


/**
 * 
 */
#define QSS_DEFINE_CLASS_ADAPTOR_SHORTCUT(CLASS, TAGS_TUPLE, MEMBER_SEQ, ... /* GLOBAL_SEQ */) \
    QSS_DEFINE_CLASS_ADAPTOR(CLASS, QSS_UNROLL_SHORTCUT_SEQ(TAGS_TUPLE, MEMBER_SEQ), __VA_ARGS__)

#define QSS_DEFINE_CLASS_ADAPTOR_SHORTCUT_GSN(CLASS, MEMBER_SEQ, ... /* GLOBAL_SEQ */) \
    QSS_DEFINE_CLASS_ADAPTOR_SHORTCUT(CLASS, (getter, setter, name), MEMBER_SEQ, __VA_ARGS__)


/**
 * \internal
 * 
 * Unrolls a member sequence that uses tag shortcuts into its standard form.
 * 
 * For example, the following invocation:
 * 
 * \code
 * QSS_UNROLL_SHORTCUT_SEQ(
 *     (getter, setter, name),
 *     ((&QSize::width, &QSize::setWidth, lit("width"))
 *     ((&QSize::height, &QSize::setHeight, lit("height"))(optional, true))
 * )
 * \endcode
 * 
 * Will expand into:
 *
 * \code
 * ((getter, &QSize::width)(setter, &QSize::setWidth)(name, lit("width"))
 * ((getter, &QSize::height)(setter, &QSize::setHeight)(name, lit("height")(optional, true))
 * \endcode
 */
#define QSS_UNROLL_SHORTCUT_SEQ(TAGS_TUPLE, MEMBER_SEQ)                         \
    BOOST_PP_SEQ_FOR_EACH(QSS_UNROLL_SHORTCUT_SEQ_STEP_I, TAGS_TUPLE, MEMBER_SEQ)

#define QSS_UNROLL_SHORTCUT_SEQ_STEP_I(R, TAGS_TUPLE, PROPERTY_SEQ)             \
    QSS_UNROLL_SHORTCUT_SEQ_STEP_II(TAGS_TUPLE, (BOOST_PP_VARIADIC_SEQ_HEAD(PROPERTY_SEQ))) BOOST_PP_VARIADIC_SEQ_TAIL(PROPERTY_SEQ)

#define QSS_UNROLL_SHORTCUT_SEQ_STEP_II(TAGS_TUPLE, VALUES_TUPLE)               \
    QSS_RANGE_FOR_EACH(QSS_UNROLL_SHORTCUT_SEQ_STEP_STEP_I, (TAGS_TUPLE, VALUES_TUPLE), (0, BOOST_PP_TUPLE_SIZE(TAGS_TUPLE)))

#define QSS_UNROLL_SHORTCUT_SEQ_STEP_STEP_I(R, DATA, INDEX)                     \
    QSS_UNROLL_SHORTCUT_SEQ_STEP_STEP_II(BOOST_PP_TUPLE_ELEM(0, DATA), BOOST_PP_TUPLE_ELEM(1, DATA), INDEX)

#define QSS_UNROLL_SHORTCUT_SEQ_STEP_STEP_II(TAGS_TUPLE, VALUES_TUPLE, INDEX)   \
    (BOOST_PP_TUPLE_ELEM(INDEX, TAGS_TUPLE), BOOST_PP_TUPLE_ELEM(INDEX, VALUES_TUPLE))


/**
 * \internal
 * 
 * This macro iterates over an integer range defined as a <tt>(begin, end)</tt> 
 * tuple. E.g. for <tt>(1, 3)</tt> range it will expand to:
 * \code
 * MACRO(1)
 * MACRO(2)
 * \endcode
 * 
 * \see BOOST_PP_SEQ_FOR_EACH
 */
#define QSS_RANGE_FOR_EACH(MACRO, DATA, RANGE)                                  \
    BOOST_PP_FOR((BOOST_PP_TUPLE_ENUM(RANGE), MACRO, DATA), QSS_RANGE_FOR_EACH_P, QSS_RANGE_FOR_EACH_O, QSS_RANGE_FOR_EACH_M)

#define QSS_RANGE_FOR_EACH_P(R, STATE)                                          \
    BOOST_PP_NOT_EQUAL(                                                         \
        BOOST_PP_TUPLE_ELEM(0, STATE),                                          \
        BOOST_PP_TUPLE_ELEM(1, STATE)                                           \
    )

#define QSS_RANGE_FOR_EACH_O(R, STATE)                                          \
    (                                                                           \
        BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(0, STATE)),                            \
        BOOST_PP_TUPLE_ELEM(1, STATE),                                          \
        BOOST_PP_TUPLE_ELEM(2, STATE),                                          \
        BOOST_PP_TUPLE_ELEM(3, STATE)                                           \
    )

#define QSS_RANGE_FOR_EACH_M(R, STATE)                                          \
    BOOST_PP_TUPLE_ELEM(2, STATE)(R, BOOST_PP_TUPLE_ELEM(3, STATE), BOOST_PP_TUPLE_ELEM(0, STATE))


/**
 * \internal
 *
 * This macro returns a type expression that can be used in return type of a
 * tag getter function. By default it simply returns <tt>decltype(VALUE)</tt>.
 * 
 * To override the default return value for some tag <tt>some_tag</tt>, use
 * the following code:
 * \code
 * #define QSS_TAG_IS_TYPED_FOR_some_tag ,
 * #define QSS_TAG_TYPE_FOR_some_tag QString // Or your custom type
 * \endcode
 * 
 * This might be useful when <tt>decltype</tt> cannot be used because
 * <tt>VALUE</tt> might be a lambda.
 *
 * \param TAG                           Qss tag.
 * \param VALUE                         Value specified by the user for this tag.
 * \returns                             Type expression that should be used for
 *                                      return type of tag getter function.
 */
#define QSS_TAG_TYPE(TAG, VALUE)                                                \
    BOOST_PP_OVERLOAD(QSS_TAG_TYPE_I_, ~ BOOST_PP_CAT(QSS_TAG_IS_TYPED_FOR_, TAG) ~)(TAG, VALUE)
    
#define QSS_TAG_TYPE_I_1(TAG, VALUE) decltype(QSS_TAG_VALUE(TAG, VALUE))
#define QSS_TAG_TYPE_I_2(TAG, VALUE) BOOST_PP_CAT(QSS_TAG_TYPE_FOR_, TAG)

/**
 * \internal
 * 
 * This macro returns an expression that can be used in return statement
 * of a tag getter function. By default it simply returns <tt>VALUE</tt>.
 * 
 * To override the default return value for some tag <tt>some_tag</tt>, use
 * the following code:
 * \code
 * #define QSS_TAG_IS_WRAPPED_FOR_some_tag ,
 * #define QSS_TAG_WRAPPER_FOR_some_tag QLatin1Literal // Or your custom wrapper macro
 * \endcode
 * 
 * \param TAG                           Qss tag.
 * \param VALUE                         Value specified by the user for this tag.
 * \returns                             Expression that should be used in the
 *                                      return statement of tag getter function.
 */
#define QSS_TAG_VALUE(TAG, VALUE)                                               \
    BOOST_PP_OVERLOAD(QSS_TAG_VALUE_I_, ~ BOOST_PP_CAT(QSS_TAG_IS_WRAPPED_FOR_, TAG) ~)(TAG, VALUE)

#define QSS_TAG_VALUE_I_1(TAG, VALUE) VALUE
#define QSS_TAG_VALUE_I_2(TAG, VALUE) BOOST_PP_CAT(QSS_TAG_WRAPPER_FOR_, TAG)(VALUE)


/**
 * \internal
 *
 * This macro transforms the elements of the provided sequence into
 * <tt>(index, element)</tt> tuples.
 * 
 * Example input: 
 * \code
 * (a)(b)(c)(d)
 * \endcode
 * 
 * Example output:
 * \code
 * ((0, a))((1, b))((2, c))((3, d))
 * \endcode
 * 
 * \param SEQ                           Sequence to transform.
 * \returns                             Transformed sequence.
 */
#define QSS_ENUM_SEQ(SEQ)                                                       \
    BOOST_PP_TUPLE_ELEM(                                                        \
        1,                                                                      \
        BOOST_PP_SEQ_FOLD_LEFT(QSS_ENUM_SEQ_STEP_I, (0, BOOST_PP_SEQ_NIL), SEQ) \
    )

#define QSS_ENUM_SEQ_STEP_I(S, STATE, ELEM)                                     \
    (                                                                           \
        BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(0, STATE)),                            \
        BOOST_PP_TUPLE_ELEM(1, STATE)((BOOST_PP_TUPLE_ELEM(0, STATE), ELEM))    \
    )


namespace QssDetail {
    /* We place the conditional serialization functions in the same namespace
     * where ADL lookup takes place. */

    template<class T, class D>
    typename boost::enable_if<boost::mpl::and_<Qss::has_visit_members<T>, Qss::has_serialization_visitor_type<D> >, void>::type
    serialize(const T &value, D *target) {
        typename Qss::serialization_visitor_type<D>::type visitor(*target);
        Qss::visit_members(value, visitor);
    }

    template<class T, class D>
    typename boost::enable_if<boost::mpl::and_<Qss::has_visit_members<T>, Qss::has_deserialization_visitor_type<D> >, bool>::type
    deserialize(const D &value, T *target) {
        typename Qss::deserialization_visitor_type<D>::type visitor(value);
        return Qss::visit_members(*target, visitor);
    }

} // namespace QssDetail


#endif // QN_SERIALIZATION_FUNCTIONS_H
