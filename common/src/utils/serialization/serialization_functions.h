#ifndef QN_SERIALIZATION_FUNCTIONS_H
#define QN_SERIALIZATION_FUNCTIONS_H

#include <utility> /* For std::forward and std::declval. */

#include <utils/preprocessor/variadic_seq_for_each.h>

#include <boost/preprocessor/seq/fold_left.hpp>

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
     * This class is the external interface that is to be used by visitor
     * classes.
     */
    template<class MemberAdaptor>
    struct AdaptorWrapper {
        template<class Tag>
        static decltype(MemberAdaptor::get(Tag())) 
        get() {
            return MemberAdaptor::get(Tag());
        }

        template<class Tag, class Default>
        static typename QssDetail::DefaultGetter<AdaptorWrapper, Tag, Default>::result_type
        get() {
            return QssDetail::DefaultGetter<AdaptorWrapper, Tag, Default>()();
        }
    };

    template<class D>
    struct serialization_visitor_type {};

    template<class D>
    struct deserialization_visitor_type {};

    template<class T, class Visitor>
    bool visit_members(T &&value, Visitor &&visitor) {
        return QssDetail::visit_members_internal(std::forward<T>(value), std::forward<Visitor>(visitor));
    }

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
#define QSS_TAG_IS_TYPED_FOR_optional ,
#define QSS_TAG_TYPE_FOR_optional bool


/**
 * This macro registers Qss visitor that is to be used when serializing
 * and deserializing to/from the given data class. This macro must be used 
 * in global namespace.
 * 
 * Defining the visitor makes all Qss adapted classes instantly (de)serializable
 * to/from the given data class.
 * 
 * \param DATA_CLASS                    Data class to define visitor for.
 * \param SERIALIZATION_VISITOR         Serialization visitor class.
 * \param DESERIALIZATION_VISITOR       Deserialization visitor class.
 */
#define QSS_REGISTER_VISITORS(DATA_CLASS, SERIALIZATION_VISITOR, DESERIALIZATION_VISITOR) \
    QSS_REGISTER_SERIALIZATION_VISITOR(DATA_CLASS, SERIALIZATION_VISITOR)       \
    QSS_REGISTER_DESERIALIZATION_VISITOR(DATA_CLASS, DESERIALIZATION_VISITOR)

#define QSS_REGISTER_SERIALIZATION_VISITOR(DATA_CLASS, SERIALIZATION_VISITOR)   \
namespace Qss {                                                                 \
    template<>                                                                  \
    struct serialization_visitor_type<DATA_CLASS>:                              \
        boost::mpl::identity<SERIALIZATION_VISITOR>                             \
    {};                                                                         \
}

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
#define QSS_DEFINE_CLASS_ADAPTOR(CLASS, MEMBER_SEQ, GLOBAL_SEQ)                 \
    QSS_DEFINE_CLASS_ADAPTOR_I(CLASS, QSS_ENUM_SEQ(MEMBER_SEQ), GLOBAL_SEQ)

#define QSS_DEFINE_CLASS_ADAPTOR_I(CLASS, MEMBER_SEQ, GLOBAL_SEQ)               \
template<class T>                                                               \
struct QssMemberAdaptor;                                                        \
                                                                                \
template<>                                                                      \
struct QssMemberAdaptor<CLASS> {                                                \
    BOOST_PP_SEQ_FOR_EACH(QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_I, GLOBAL_SEQ, MEMBER_SEQ) \
                                                                                \
    template<class T, class Visitor>                                            \
    static bool visit_members(T &&value, Visitor &&visitor) {                   \
        BOOST_PP_SEQ_FOR_EACH(QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_I, GLOBAL_SEQ, MEMBER_SEQ) \
        return true;                                                            \
    }                                                                           \
};                                                                              \
                                                                                \
template<class Visitor>                                                         \
bool visit_members(const CLASS &value, Visitor &&visitor) {                     \
    return QssMemberAdaptor<CLASS>::visit_members(value, std::forward<Visitor>(visitor)); \
}                                                                               \
                                                                                \
template<class Visitor>                                                         \
bool visit_members(CLASS &value, Visitor &&visitor) {                           \
    return QssMemberAdaptor<CLASS>::visit_members(value, std::forward<Visitor>(visitor)); \
}


#define QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_I(R, GLOBAL_SEQ, PROPERTY_TUPLE)   \
    QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_II(                                    \
        BOOST_PP_TUPLE_ELEM(0, PROPERTY_TUPLE),                                 \
        GLOBAL_SEQ BOOST_PP_TUPLE_ELEM(1, PROPERTY_TUPLE)                       \
    )

#define QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_II(INDEX, PROPERTY_SEQ)            \
    struct BOOST_PP_CAT(Member, INDEX) {                                        \
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
    struct has_tag<TAG>:                                                        \
        boost::mpl::true_type                                                   \
    {};                                                                         \
                                                                                \
    static QSS_TAG_TYPE(TAG, VALUE) get(const Qss::TAG &) {                     \
        return VALUE;                                                           \
    }


#define QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_I(R, GLOBAL_SEQ, PROPERTY_TUPLE) \
    QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_II(                                  \
        BOOST_PP_TUPLE_ELEM(0, PROPERTY_TUPLE),                                 \
        GLOBAL_SEQ BOOST_PP_TUPLE_ELEM(1, PROPERTY_TUPLE)                       \
    )

#define QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_II(INDEX, PROPERTY_SEQ)          \
    if(!visitor(std::forward<T>(value), Qss::AdaptorWrapper<BOOST_PP_CAT(Member, INDEX)>())) \
        return false;


/**
 * \internal
 *
 * This macro returns a type expression that can be used in return type of a
 * tag getter function. By default it simply returns <tt>decltype(VALUE)</tt>,
 * but in some cases this is not a valid expression (e.g. when <tt>VALUE</tt>
 * is a lambda). For such cases this macro provides an extension mechanism
 * that makes it possible to specify the type for the tag explicitly at
 * tag definition time.
 *
 * \param TAG                           Qss tag.
 * \param VALUE                         Value specified by the user for this tag.
 * \returns                             Type expression that should be used for
 *                                      return type of tag getter function.
 */
#define QSS_TAG_TYPE(TAG, VALUE)                                                \
    BOOST_PP_OVERLOAD(QSS_TAG_TYPE_I_, ~ BOOST_PP_CAT(QSS_TAG_IS_TYPED_FOR_, TAG) ~)(TAG, VALUE)
    
#define QSS_TAG_TYPE_I_1(TAG, VALUE) decltype(VALUE)
#define QSS_TAG_TYPE_I_2(TAG, VALUE) BOOST_PP_CAT(QSS_TAG_TYPE_FOR_, TAG)


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
