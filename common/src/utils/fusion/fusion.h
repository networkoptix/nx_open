#ifndef QN_FUSION_H
#define QN_FUSION_H

#include <utility> /* For std::forward and std::declval. */

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
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

    template<class Adaptor, class Tag, class T, bool hasTag = Adaptor::template has_tag<Tag>::value>
    struct DefaultGetter {
        typedef decltype(Adaptor::get(Tag())) result_type;
        result_type operator()(const T &) const {
            return Adaptor::get(Tag());
        }
    };

    template<class Adaptor, class Tag, class T>
    struct DefaultGetter<Adaptor, Tag, T, false> {
        typedef T result_type;
        result_type operator()(const T &defaultValue) const {
            return defaultValue;
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
        get(const Tag &) {
            return Adaptor::get(Tag());
        }

        template<class Tag, class T>
        static typename QnFusionDetail::DefaultGetter<Adaptor, Tag, T>::result_type
        get(const Tag &, const T &defaultValue) {
            return QnFusionDetail::DefaultGetter<Adaptor, Tag, T>()(defaultValue);
        }
    };

    template<class T>
    struct has_visit_members: 
        QnFusionDetail::has_visit_members<T>
    {};

} // namespace QnFusion


/**
 * This macro defines a new fusion tag. It must be used in global namespace.
 * Defined tag can then be accessed from the QnFusion namespace.
 * 
 * \param TAG                           Tag to define.
 */
#define QN_FUSION_DEFINE_TAG(TAG)                                               \
namespace QnFusion {                                                            \
    struct BOOST_PP_CAT(TAG, _tag) {};                                          \
    namespace {                                                                 \
        const BOOST_PP_CAT(TAG, _tag) TAG = {};                                 \
    }                                                                           \
}

/**
 * \param TAG                           Fusion tag.
 * \returns                             C++ type of the provided fusion tag.
 */
#define QN_FUSION_TAG_TYPE(TAG)                                                 \
    QnFusion::BOOST_PP_CAT(TAG, _tag)

QN_FUSION_DEFINE_TAG(index)
QN_FUSION_DEFINE_TAG(getter)
QN_FUSION_DEFINE_TAG(setter)
QN_FUSION_DEFINE_TAG(checker)
QN_FUSION_DEFINE_TAG(name)
QN_FUSION_DEFINE_TAG(optional)

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
