#ifndef QN_FUSION_H
#define QN_FUSION_H

#include <utility> /* For std::forward and std::declval. */
#include <type_traits> /* For std::remove_*. */

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/sizeof.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/integral_c.hpp>
#endif // Q_MOC_RUN

#include <utils/preprocessor/variadic_seq_for_each.h>

#include "fusion_fwd.h"

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
    public:
        typedef MemberAdaptor<Adaptor> this_type;

        template<class T>
        struct result;

        template<class F, class T0>
        struct result<F(T0)>:
            Adaptor::template result<Adaptor(T0)>
        {};

        template<class F, class T0, class T1>
        struct result<F(T0, T1)>:
            boost::mpl::if_<
                typename Adaptor::template has_key<T0>::type,
                result<F(T0)>,
                boost::mpl::identity<T1>
            >::type
        {};

        template<class Key>
        typename result<this_type(Key)>::type operator()(const Key &key) const {
            return Adaptor()(key);
        }

        template<class Key, class T>
        typename result<this_type(Key, T)>::type operator()(const Key &key, const T &, const typename boost::enable_if<typename Adaptor::template has_key<Key>::type>::type * = NULL) const {
            return operator()(key);
        }

        template<class Key, class T>
        typename result<this_type(Key, T)>::type operator()(const Key &, const T &defaultValue, const typename boost::disable_if<typename Adaptor::template has_key<Key>::type>::type * = NULL) const {
            return defaultValue;
        }
    };

    template<class T>
    struct has_visit_members: 
        QnFusionDetail::has_visit_members<T>
    {};

} // namespace QnFusion


/**
 * This macro defines a new fusion key. It must be used in global namespace.
 * Defined key can then be accessed from the QnFusion namespace.
 * 
 * \param KEY                           Key to define.
 */
#define QN_FUSION_DEFINE_KEY(KEY)                                               \
namespace QnFusion {                                                            \
    struct BOOST_PP_CAT(KEY, _type) {};                                         \
    namespace {                                                                 \
        const BOOST_PP_CAT(KEY, _type) KEY = {};                                \
    }                                                                           \
}

/**
 * \param KEY                           Fusion key.
 * \returns                             C++ type of the provided fusion key.
 */
#define QN_FUSION_KEY_TYPE(KEY)                                                 \
    QnFusion::BOOST_PP_CAT(KEY, _type)

QN_FUSION_DEFINE_KEY(index)
QN_FUSION_DEFINE_KEY(getter)
QN_FUSION_DEFINE_KEY(setter)
QN_FUSION_DEFINE_KEY(setter_tag)
QN_FUSION_DEFINE_KEY(checker)
QN_FUSION_DEFINE_KEY(name)
QN_FUSION_DEFINE_KEY(optional)

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_index ,
#define QN_FUSION_PROPERTY_TYPE_FOR_index int

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_name ,
#define QN_FUSION_PROPERTY_TYPE_FOR_name QString
#define QN_FUSION_PROPERTY_IS_WRAPPED_FOR_name ,
#define QN_FUSION_PROPERTY_WRAPPER_FOR_name lit

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_optional ,
#define QN_FUSION_PROPERTY_TYPE_FOR_optional bool


/**
 *
 */
#define QN_FUSION_DEFINE_FUNCTIONS(CLASS, FUNCTION_SEQ, ... /* PREFIX */)       \
    BOOST_PP_SEQ_FOR_EACH(QN_FUSION_DEFINE_FUNCTIONS_STEP_I, (CLASS, ##__VA_ARGS__), FUNCTION_SEQ)

#define QN_FUSION_DEFINE_FUNCTIONS_STEP_I(R, PARAMS, FUNCTION)                  \
    BOOST_PP_CAT(QN_FUSION_DEFINE_FUNCTIONS_, FUNCTION) PARAMS


/**
 * TODO: Note why BOOST_PP_VARIADIC_SEQ_FOR_EACH
 */
#define QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(CLASS_SEQ, FUNCTION_SEQ, ... /* PREFIX */) \
    BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES_STEP_I, (FUNCTION_SEQ, ##__VA_ARGS__), CLASS_SEQ)

#define QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES_STEP_I(R, PARAMS, CLASS)           \
    QN_FUSION_DEFINE_FUNCTIONS(BOOST_PP_TUPLE_ENUM(CLASS), BOOST_PP_TUPLE_ENUM(PARAMS))


#endif // QN_FUSION_H
