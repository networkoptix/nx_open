#ifndef QN_FUSION_H
#define QN_FUSION_H

#include <utility> /* For std::forward and std::declval. */

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/seq/fold_left.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/comparison/not_equal.hpp>
#include <boost/preprocessor/tuple/size.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>

#include <boost/type_traits/is_convertible.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/sizeof.hpp>
#include <boost/mpl/has_xxx.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/integral_c.hpp>

#include <utils/preprocessor/variadic_seq_for_each.h>

namespace QnFusionDetail {
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
    struct DefaultGetter<Adaptor, Tag, Default, false> {
        typedef Default result_type;
        result_type operator()() const {
            return Default();
        }
    };

    template<class Visitor, class T>
    boost::type_traits::yes_type has_visitor_initializer_test(const Visitor &, const T &, const decltype(std::declval<Visitor>()(std::declval<T>())) * = NULL);
    boost::type_traits::no_type has_visitor_initializer_test(...);

    template<class Visitor, class T>
    struct has_visitor_initializer: 
        boost::mpl::equal_to<
            boost::mpl::sizeof_<boost::type_traits::yes_type>, 
            boost::mpl::size_t<sizeof(has_visitor_initializer_test(std::declval<Visitor>(), std::declval<T>()))>
        > 
    {};

    template<class Visitor, class T, bool hasInitializer = has_visitor_initializer<Visitor, T>::value>
    struct VisitorInitializer {
        bool operator()(Visitor &&visitor, T &&value) const {
            return visitor(std::forward<T>(value));
        }
    };

    template<class Visitor, class T>
    struct VisitorInitializer<Visitor, T, false> {
        bool operator()(Visitor &&, T &&) const {
            return true;
        }
    };

    template<class Visitor, class T>
    bool initialize_visitor(Visitor &&visitor, T &&value) {
        return VisitorInitializer<Visitor, T>()(std::forward<Visitor>(visitor), std::forward<T>(value));
    }
   

} // namespace QnFusionDetail

namespace QnFusion {
    /**
     * Main API entry point. Iterates through the members of a previously 
     * registered type.
     * 
     * Visitor is expected to define <tt>operator()</tt> of the following form:
     * \code
     * template<class T, class Adaptor>
     * bool operator()(const T &, const Adaptor &);
     * \endcode
     *
     * Here <tt>Adaptor</tt> template parameter presents an interface defined
     * by <tt>MemberAdaptor</tt> class. Return value of false indicates that
     * iteration should be stopped, and in this case the function will
     * return false.
     *
     * Visitor can also optionally define <tt>operator()(const T &)</tt> that will then 
     * be invoked before the iteration takes place. If this operator returns 
     * false, then iteration won't start and the function will also return false.
     *
     * \param value                     Value to iterate through.
     * \param visitor                   Visitor class to apply to members.
     * \see MemberAdaptor
     */
    template<class T, class Visitor>
    bool visit_members(T &&value, Visitor &&visitor) {
        return QnFusionDetail::visit_members_internal(std::forward<T>(value), std::forward<Visitor>(visitor));
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
        static typename QnFusionDetail::DefaultGetter<Adaptor, Tag, Default>::result_type
        get() {
            return QnFusionDetail::DefaultGetter<Adaptor, Tag, Default>()();
        }
    };

    /**
     * \param D                         Data type.
     * \returns                         Serialization visitor type that was
     *                                  registered for the given data type.
     * \see QN_FUSION_REGISTER_VISITORS
     */
    template<class D>
    struct serialization_visitor_type {};

    /**
     * \param D                         Data type.
     * \returns                         Deserialization visitor type that was
     *                                  registered for the given data type.
     * \see QN_FUSION_REGISTER_VISITORS
     */
    template<class D>
    struct deserialization_visitor_type {};

    template<class D>
    struct has_serialization_visitor_type:
        QnFusionDetail::has_type<serialization_visitor_type<D> >
    {};

    template<class D>
    struct has_deserialization_visitor_type:
        QnFusionDetail::has_type<deserialization_visitor_type<D> >
    {};

    template<class T>
    struct has_visit_members: 
        QnFusionDetail::has_visit_members<T>
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

} // namespace QnFusion


/**
 * This macro defines a new fusion tag. It must be used in global namespace.
 * Defined tag can then be accessed from the QnFusion namespace.
 * 
 * \param TAG                           Tag to define.
 */
#define QN_FUSION_DEFINE_TAG(TAG)                                               \
namespace QnFusion { struct TAG {}; }

QN_FUSION_DEFINE_TAG(index)
QN_FUSION_DEFINE_TAG(getter)
QN_FUSION_DEFINE_TAG(setter)
QN_FUSION_DEFINE_TAG(checker)
QN_FUSION_DEFINE_TAG(name)
QN_FUSION_DEFINE_TAG(optional)

#define QN_FUSION_TAG_IS_TYPED_FOR_index ,
#define QN_FUSION_TAG_TYPE_FOR_index int

#define QN_FUSION_TAG_IS_TYPED_FOR_name ,
#define QN_FUSION_TAG_TYPE_FOR_name QString
#define QN_FUSION_TAG_IS_WRAPPED_FOR_name ,
#define QN_FUSION_TAG_WRAPPER_FOR_name lit

#define QN_FUSION_TAG_IS_TYPED_FOR_optional ,
#define QN_FUSION_TAG_TYPE_FOR_optional bool


/**
 * This macro registers fusion visitors that are to be used when serializing
 * and deserializing to/from the given data class. This macro must be used 
 * in global namespace.
 * 
 * Defining the visitors makes all fusion adapted classes instantly (de)serializable
 * to/from the given data class.
 * 
 * \param DATA_CLASS                    Data class to define visitors for.
 * \param SERIALIZATION_VISITOR         Serialization visitor class.
 * \param DESERIALIZATION_VISITOR       Deserialization visitor class.
 */
#define QN_FUSION_REGISTER_VISITORS(DATA_CLASS, SERIALIZATION_VISITOR, DESERIALIZATION_VISITOR) \
    QN_FUSION_REGISTER_SERIALIZATION_VISITOR(DATA_CLASS, SERIALIZATION_VISITOR) \
    QN_FUSION_REGISTER_DESERIALIZATION_VISITOR(DATA_CLASS, DESERIALIZATION_VISITOR)

/**
 * \see QN_FUSION_REGISTER_VISITORS
 */
#define QN_FUSION_REGISTER_SERIALIZATION_VISITOR(DATA_CLASS, SERIALIZATION_VISITOR) \
namespace QnFusion {                                                            \
    template<>                                                                  \
    struct serialization_visitor_type<DATA_CLASS>:                              \
        boost::mpl::identity<SERIALIZATION_VISITOR>                             \
    {};                                                                         \
}

/**
 * \see QN_FUSION_REGISTER_VISITORS
 */
#define QN_FUSION_REGISTER_DESERIALIZATION_VISITOR(DATA_CLASS, DESERIALIZATION_VISITOR) \
namespace QnFusion {                                                            \
    template<>                                                                  \
    struct deserialization_visitor_type<DATA_CLASS>:                            \
        boost::mpl::identity<DESERIALIZATION_VISITOR>                           \
    {};                                                                         \
}


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
        template<class Tag>                                                     \
        struct has_tag:                                                         \
            boost::mpl::false_                                                  \
        {};                                                                     \
                                                                                \
        BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_I, ~, PROPERTY_SEQ (index, INDEX)) \
    };

#define QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_I(R, DATA, PROPERTY_TUPLE)       \
    QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_II PROPERTY_TUPLE

#define QN_FUSION_ADAPT_CLASS_OBJECT_STEP_STEP_II(TAG, VALUE)                   \
    template<>                                                                  \
    struct has_tag<QnFusion::TAG>:                                              \
        boost::mpl::true_                                                       \
    {};                                                                         \
                                                                                \
    static QN_FUSION_TAG_TYPE(TAG, VALUE) get(const QnFusion::TAG &) {          \
        return QN_FUSION_TAG_VALUE(TAG, VALUE);                                 \
    }


#define QN_FUSION_ADAPT_CLASS_FUNCTION_STEP_I(Z, INDEX, DATA)                   \
    if(!visitor(std::forward<T>(value), QnFusion::MemberAdaptor<BOOST_PP_CAT(MemberAdaptor, INDEX)>())) \
        return false;


/**
 * 
 */
#define QN_FUSION_ADAPT_CLASS_SHORT(CLASS, TAGS_TUPLE, MEMBER_SEQ, ... /* GLOBAL_SEQ */) \
    QN_FUSION_ADAPT_CLASS(CLASS, QN_FUSION_UNROLL_SHORTCUT_SEQ(TAGS_TUPLE, MEMBER_SEQ), ##__VA_ARGS__)

#define QN_FUSION_ADAPT_CLASS_GSN(CLASS, MEMBER_SEQ, ... /* GLOBAL_SEQ */) \
    QN_FUSION_ADAPT_CLASS_SHORT(CLASS, (getter, setter, name), MEMBER_SEQ, ##__VA_ARGS__)

#define QN_FUSION_ADAPT_CLASS_GSNC(CLASS, MEMBER_SEQ, ... /* GLOBAL_SEQ */) \
    QN_FUSION_ADAPT_CLASS_SHORT(CLASS, (getter, setter, name, checker), MEMBER_SEQ, ##__VA_ARGS__)


/**
 *
 */
#define QN_FUSION_ADAPT_STRUCT(STRUCT, FIELD_SEQ, ... /* GLOBAL_SEQ */) \
    QN_FUSION_ADAPT_CLASS(STRUCT, QN_FUSION_FIELD_SEQ_TO_MEMBER_SEQ(STRUCT, FIELD_SEQ), ##__VA_ARGS__)


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
#define QN_FUSION_UNROLL_SHORTCUT_SEQ(TAGS_TUPLE, MEMBER_SEQ)                   \
    BOOST_PP_SEQ_FOR_EACH(QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_I, TAGS_TUPLE, MEMBER_SEQ)

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_I(R, TAGS_TUPLE, PROPERTY_SEQ)       \
    QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_II(TAGS_TUPLE, (BOOST_PP_VARIADIC_SEQ_HEAD(PROPERTY_SEQ))) BOOST_PP_VARIADIC_SEQ_TAIL(PROPERTY_SEQ)

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_II(TAGS_TUPLE, VALUES_TUPLE)         \
    BOOST_PP_REPEAT(BOOST_PP_TUPLE_SIZE(TAGS_TUPLE), QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_I, (TAGS_TUPLE, VALUES_TUPLE))

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_I(Z, INDEX, DATA)               \
    QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_II(BOOST_PP_TUPLE_ELEM(0, DATA), BOOST_PP_TUPLE_ELEM(1, DATA), INDEX)

#define QN_FUSION_UNROLL_SHORTCUT_SEQ_STEP_STEP_II(TAGS_TUPLE, VALUES_TUPLE, INDEX) \
    (BOOST_PP_TUPLE_ELEM(INDEX, TAGS_TUPLE), BOOST_PP_TUPLE_ELEM(INDEX, VALUES_TUPLE))


/**
 * \internal
 *
 * This macro returns a type expression that can be used in return type of a
 * tag getter function. By default it simply returns <tt>decltype(VALUE)</tt>.
 * 
 * To override the default return value for some tag <tt>some_tag</tt>, use
 * the following code:
 * \code
 * #define QN_FUSION_TAG_IS_TYPED_FOR_some_tag ,
 * #define QN_FUSION_TAG_TYPE_FOR_some_tag QString // Or your custom type
 * \endcode
 * 
 * This might be useful when <tt>decltype</tt> cannot be used because
 * <tt>VALUE</tt> might be a lambda.
 *
 * \param TAG                           Fusion tag.
 * \param VALUE                         Value specified by the user for this tag.
 * \returns                             Type expression that should be used for
 *                                      return type of tag getter function.
 */
#define QN_FUSION_TAG_TYPE(TAG, VALUE)                                          \
    BOOST_PP_OVERLOAD(QN_FUSION_TAG_TYPE_I_, ~ BOOST_PP_CAT(QN_FUSION_TAG_IS_TYPED_FOR_, TAG) ~)(TAG, VALUE)
    
#define QN_FUSION_TAG_TYPE_I_1(TAG, VALUE) decltype(QN_FUSION_TAG_VALUE(TAG, VALUE))
#define QN_FUSION_TAG_TYPE_I_2(TAG, VALUE) BOOST_PP_CAT(QN_FUSION_TAG_TYPE_FOR_, TAG)


/**
 * \internal
 * 
 * This macro returns an expression that can be used in return statement
 * of a tag getter function. By default it simply returns <tt>VALUE</tt>.
 * 
 * To override the default return value for some tag <tt>some_tag</tt>, use
 * the following code:
 * \code
 * #define QN_FUSION_TAG_IS_WRAPPED_FOR_some_tag ,
 * #define QN_FUSION_TAG_WRAPPER_FOR_some_tag QLatin1Literal // Or your custom wrapper macro
 * \endcode
 * 
 * \param TAG                           Fusion tag.
 * \param VALUE                         Value specified by the user for this tag.
 * \returns                             Expression that should be used in the
 *                                      return statement of tag getter function.
 */
#define QN_FUSION_TAG_VALUE(TAG, VALUE)                                         \
    BOOST_PP_OVERLOAD(QN_FUSION_TAG_VALUE_I_, ~ BOOST_PP_CAT(QN_FUSION_TAG_IS_WRAPPED_FOR_, TAG) ~)(TAG, VALUE)

#define QN_FUSION_TAG_VALUE_I_1(TAG, VALUE) VALUE
#define QN_FUSION_TAG_VALUE_I_2(TAG, VALUE) BOOST_PP_CAT(QN_FUSION_TAG_WRAPPER_FOR_, TAG)(VALUE)


namespace QnFusion {
    ///* We place the conditional serialization functions in the same namespace
    // * where ADL lookup takes place. */
    // TODO: These go into Qss, then pulled into QnSerializationDetail

    template<class T, class D>
    typename boost::enable_if<boost::mpl::and_<QnFusion::has_visit_members<T>, QnFusion::has_serialization_visitor_type<D> >, void>::type
    serialize(const T &value, D *target) {
        typename QnFusion::serialization_visitor_type<D>::type visitor(*target);
        QnFusion::visit_members(value, visitor);
    }

    template<class T, class D>
    typename boost::enable_if<boost::mpl::and_<QnFusion::has_visit_members<T>, QnFusion::has_deserialization_visitor_type<D> >, bool>::type
    deserialize(const D &value, T *target) {
        typename QnFusion::deserialization_visitor_type<D>::type visitor(value);
        return QnFusion::visit_members(*target, visitor);
    }

    template<class T, class D, class Context>
    typename boost::enable_if<boost::mpl::and_<QnFusion::has_visit_members<T>, QnFusion::has_serialization_visitor_type<D> >, void>::type
    serialize(Context *ctx, const T &value, D *target) {
        typename QnFusion::serialization_visitor_type<D>::type visitor(ctx, *target);
        QnFusion::visit_members(value, visitor);
    }

    template<class T, class D, class Context>
    typename boost::enable_if<boost::mpl::and_<QnFusion::has_visit_members<T>, QnFusion::has_deserialization_visitor_type<D> >, bool>::type
    deserialize(Context *ctx, const D &value, T *target) {
        typename QnFusion::deserialization_visitor_type<D>::type visitor(ctx, value);
        return QnFusion::visit_members(*target, visitor);
    }

} // namespace QnFusion

#endif // QN_FUSION_H
