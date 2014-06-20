#ifndef QN_FUSION_H
#define QN_FUSION_H

#include <utility> /* For std::forward and std::declval. */
#include <type_traits> /* For std::enable_if, std::is_same, std::integral_constant. */

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/mpl/if.hpp>
#endif // Q_MOC_RUN

#include <utils/preprocessor/variadic_seq_for_each.h>
#include <utils/common/type_traits.h>

#include "fusion_fwd.h"
#include "fusion_detail.h"
#include "fusion_keys.h"

namespace QnFusion {

    /**
     * Main API entry point. Iterates through the members of a previously 
     * registered type.
     * 
     * Visitor is expected to define <tt>operator()</tt> of the following form:
     * \code
     * template<class T, class Access>
     * bool operator()(const T &, const Access &);
     * \endcode
     *
     * Here <tt>Access</tt> template parameter presents an access interface, 
     * defined by <tt>AccessAdaptor</tt> class. Return value of false indicates 
     * that iteration should be stopped, and in this case this function will
     * return false.
     *
     * Visitor can also optionally define <tt>operator()</tt> with additinal
     * parameter <tt>QnFusion::start_tag</tt> (or <tt>QnFusion::end_tag</tt>).
     * This operator will then be invoked before (or after) the iteration takes place. 
     * If this operator returns false, then iteration will stop and this 
     * function will also return false.
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
     * \fn invoke
     * 
     * This set of overloaded functions presents an interface for invoking 
     * setters and getters returned by member adaptors.
     */

    template<class Base, class Derived, class T>
    const T &invoke(T Base::*getter, const Derived &object) {
        return object.*getter;
    }

    template<class Base, class Derived, class T>
    T invoke(T (Base::*getter)() const, const Derived &object) {
        return (object.*getter)();
    }

    template<class Getter, class Class>
    auto invoke(const Getter &getter, const Class &object) -> decltype(getter(object)) {
        return getter(object);
    }

    template<class Base, class Derived, class T>
    void invoke(T Base::*setter, Derived &object, T &&value) {
        object.*setter = std::forward<T>(value);
    }

    template<class Base, class Derived, class R, class P, class T>
    void invoke(R (Base::*setter)(P), Derived &object, T &&value) {
        (object.*setter)(std::forward<T>(value));
    }

    template<class Setter, class Class, class T>
    void invoke(const Setter &setter, Class &object, T &&value) {
        setter(object, std::forward<T>(value));
    }

    struct start_tag {};
    struct end_tag {};

    struct member_setter_tag {};
    struct function_setter_tag {};
    template<class T>
    struct typed_setter_tag {};

    template<class T>
    struct typed_member_setter_tag: 
        member_setter_tag,
        typed_setter_tag<T>
    {};

    template<class T>
    struct typed_function_setter_tag: 
        function_setter_tag,
        typed_setter_tag<T>
    {};

    /**
     * This metafunction returns a category of the given setter.
     *
     * \tparam Setter                   Setter type.
     * \tparam T                        Setter parameter type.
     */
    template<class Setter, class T>
    struct setter_category: 
        boost::mpl::if_<
            std::is_member_object_pointer<Setter>,
            typed_member_setter_tag<T>,
            typed_function_setter_tag<T>
        >
    {
        //static_assert(std::is_member_object_pointer<Setter>::value, "111");
    };

    /**
     * This metafuction returns the type of the field specified by the given
     * access type.
     * 
     * \param Access                    Access type.
     */
    template<class Access>
    struct access_member_type:
        QnTypeTraits::remove_cvr<decltype(invoke(
            std::declval<typename Access::template at<getter_type, void>::type::result_type>(), 
            std::declval<typename Access::template at<object_declval_type, void>::type::result_type>()
        ))>
    {}; // TODO: #Elric can be simplified with std::declval<Access>

    /**
     * This metafunction returns a category of the setter specified by the
     * given access type.
     * 
     * \tparam Access                   Access type.
     * \see QnFusion::AccessAdaptor
     */
    template<class Access>
    struct access_setter_category:
        setter_category<
            typename Access::template at<setter_type, void>::type::result_type,
            typename access_member_type<Access>::type
        >
    {};


    /**
     * This class defines the access interface, which is an external interface 
     * that is to be used by visitor classes.
     */
    template<class Base>
    struct AccessAdaptor {
    public:
        template<class Key, class Default> 
        struct at: 
            Base::template at<Key, Default>
        {};

        template<class Key>
        struct has_key:
            std::integral_constant<bool, !std::is_same<typename at<Key, void>::type, void>::value>
        {};

        template<class Key>
        typename at<Key, void>::type::result_type operator()(const Key &) const {
            return typename at<Key, void>::type()();
        }

        template<class Key, class T>
        typename at<Key, void>::type::result_type operator()(const Key &, const T &, const typename std::enable_if<has_key<Key>::value>::type * = NULL) const {
            return typename at<Key, void>::type()();
        }

        template<class Key, class T>
        T operator()(const Key &, const T &defaultValue, const typename std::enable_if<!has_key<Key>::value>::type * = NULL) const {
            return defaultValue;
        }
    };


    /**
     * This metafunction returns whether the given type is fusion-adapted.
     * 
     * \tparam T                        Type to check.
     */
    template<class T>
    struct is_adapted: 
        QnFusionDetail::is_adapted<T>
    {};

} // namespace QnFusion


namespace QnFusionDetail {

    template<class Access>
    typename QnFusion::access_setter_category<Access>::type
    make_access_setter_category(const Access &) {
        return typename QnFusion::access_setter_category<Access>::type();
    }

} // namespace QnFusionDetail


/**
 * This macro generates several functions for the given fusion-adapted type. 
 * Tokens for the functions to generate are passed in FUNCTION_SEQ parameter. 
 * Accepted tokens are:
 * <ul>
 * <li> <tt>binary</tt>         --- bns (de)serialization functions. </li>
 * <li> <tt>csv_record</tt>     --- csv record serialization functions. </li>
 * <li> <tt>datastream</tt>     --- <tt>QDataStream</tt> (de)serialization functions. </li>
 * <li> <tt>debug</tt>          --- <tt>QDebug</tt> streaming functions. </li>
 * <li> <tt>eq</tt>             --- <tt>operator==</tt> and <tt>operator!=</tt>. </li>
 * <li> <tt>json</tt>           --- json (de)serialization functions. </li>
 * <li> <tt>hash</tt>           --- <tt>qHash</tt> function. </li>
 * <li> <tt>sql_record</tt>     --- sql record bind/fetch functions. </li>
 * </ul>
 * 
 * \param TYPE                          Fusion-adapted type to define functions for.
 * \param FUNCTION_SEQ                  Preprocessor sequence of functions to define.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_FUSION_DEFINE_FUNCTIONS(CLASS, FUNCTION_SEQ, ... /* PREFIX */)       \
    BOOST_PP_SEQ_FOR_EACH(QN_FUSION_DEFINE_FUNCTIONS_STEP_I, (CLASS, ##__VA_ARGS__), FUNCTION_SEQ)

#define QN_FUSION_DEFINE_FUNCTIONS_STEP_I(R, PARAMS, FUNCTION)                  \
    BOOST_PP_CAT(QN_FUSION_DEFINE_FUNCTIONS_, FUNCTION) PARAMS


/**
 * Same as QN_FUSION_DEFINE_FUNCTIONS, but for several types.
 * 
 * \param CLASS_SEQ                     Preprocessor sequence of fusion-adapted types
 *                                      to define functions for.
 * \param FUNCTION_SEQ                  Preprocessor sequence of functions to define.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 * 
 * \see QN_FUSION_DEFINE_FUNCTIONS
 */
#define QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(CLASS_SEQ, FUNCTION_SEQ, ... /* PREFIX */) \
    BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES_STEP_I, (FUNCTION_SEQ, ##__VA_ARGS__), CLASS_SEQ)

#define QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES_STEP_I(R, PARAMS, CLASS)           \
    QN_FUSION_DEFINE_FUNCTIONS(                                                 \
        BOOST_PP_TUPLE_ENUM(CLASS),                                             \
        QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES_STEP_E_0 PARAMS,                   \
        QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES_STEP_E_1 PARAMS                    \
    )

/* These ones serve as a workaround for the fact that we cannot use BOOST_PP_TUPLE_ELEM here
 * because it is already used up the stack. */
#define QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES_STEP_E_0(FUNCTION_SEQ, ... /* PREFIX */) FUNCTION_SEQ
#define QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES_STEP_E_1(FUNCTION_SEQ, ... /* PREFIX */) __VA_ARGS__

#endif // QN_FUSION_H
