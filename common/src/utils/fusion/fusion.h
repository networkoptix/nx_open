#ifndef QN_FUSION_H
#define QN_FUSION_H

#include <utility> /* For std::forward and std::declval. */
#include <type_traits> /* For std::remove_*, std::enable_if, std::is_same, std::integral_constant. */

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/mpl/if.hpp>
#endif // Q_MOC_RUN

#include <utils/preprocessor/variadic_seq_for_each.h>

#include "fusion_fwd.h"

namespace QnFusionDetail {
    struct na {};
    struct yes_type { char dummy; };
    struct no_type { char dummy[64]; };

    template<class T, class Visitor>
    bool visit_members_internal(T &&value, Visitor &&visitor) {
        return visit_members(std::forward<T>(value), std::forward<Visitor>(visitor)); /* That's the place where ADL kicks in. */
    }

    template<class T>
    yes_type has_visit_members_test(const T &, const decltype(visit_members(std::declval<T>(), std::declval<na>())) * = NULL);
    no_type has_visit_members_test(...);

    template<class T>
    struct is_adapted: 
        std::integral_constant<bool, sizeof(yes_type) == sizeof(has_visit_members_test(std::declval<T>()))>
    {};

    template<class T>
    yes_type has_operator_call_test(const T &, const decltype(std::declval<T>()()) * = NULL);
    template<class T, class Arg0>
    yes_type has_operator_call_test(const T &, const Arg0 &, const decltype(std::declval<T>()(std::declval<Arg0>())) * = NULL);
    template<class T, class Arg0, class Arg1>
    yes_type has_operator_call_test(const T &, const Arg0 &, const Arg1 &, const decltype(std::declval<T>()(std::declval<Arg0>(), std::declval<Arg1>())) * = NULL);
    template<class T, class Arg0, class Arg1, class Arg2>
    yes_type has_operator_call_test(const T &, const Arg0 &, const Arg1 &, const Arg2 &, const decltype(std::declval<T>()(std::declval<Arg0>(), std::declval<Arg1>(), std::declval<Arg2>())) * = NULL);
    no_type has_operator_call_test(...);

    template<class T, class Arg0, class Arg1, class Arg2>
    struct sizeof_has_operator_call_test: 
        std::integral_constant<std::size_t, sizeof(has_operator_call_test(std::declval<T>(), std::declval<Arg0>(), std::declval<Arg1>(), std::declval<Arg2>()))>
    {};

    template<class T, class Arg0, class Arg1>
    struct sizeof_has_operator_call_test<T, Arg0, Arg1, na>: 
        std::integral_constant<std::size_t, sizeof(has_operator_call_test(std::declval<T>(), std::declval<Arg0>(), std::declval<Arg1>()))>
    {};

    template<class T, class Arg0>
    struct sizeof_has_operator_call_test<T, Arg0, na, na>: 
        std::integral_constant<std::size_t, sizeof(has_operator_call_test(std::declval<T>(), std::declval<Arg0>()))>
    {};

    template<class T>
    struct sizeof_has_operator_call_test<T, na, na, na>: 
        std::integral_constant<std::size_t, sizeof(has_operator_call_test(std::declval<T>()))>
    {};

    template<class T, class Arg0 = na, class Arg1 = na, class Arg2 = na>
    struct has_operator_call:
        std::integral_constant<bool, sizeof(yes_type) == sizeof_has_operator_call_test<T, Arg0, Arg1, Arg2>::value>
    {};

    template<class T, class Arg0, class Arg1, class Arg2>
    bool safe_operator_call(T &&functor, Arg0 &&arg0, Arg1 &&arg1, Arg2 &&arg2, const typename std::enable_if<has_operator_call<T, Arg0, Arg1, Arg2>::value>::type * = NULL) {
        return functor(std::forward<Arg0>(arg0), std::forward<Arg1>(arg1), std::forward<Arg2>(arg2));
    }

    template<class T, class Arg0, class Arg1, class Arg2>
    bool safe_operator_call(T &&, Arg0 &&, Arg1 &&, Arg2 &&, const typename std::enable_if<!has_operator_call<T, Arg0, Arg1, Arg2>::value>::type * = NULL) {
        return true;
    }

} // namespace QnFusionDetail


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

QN_FUSION_DEFINE_KEY(base)
QN_FUSION_DEFINE_KEY(member_index)
QN_FUSION_DEFINE_KEY(member_count)
QN_FUSION_DEFINE_KEY(object_declval)
QN_FUSION_DEFINE_KEY(getter)
QN_FUSION_DEFINE_KEY(setter)
QN_FUSION_DEFINE_KEY(setter_tag)
QN_FUSION_DEFINE_KEY(checker)
QN_FUSION_DEFINE_KEY(name)
QN_FUSION_DEFINE_KEY(c_name)
QN_FUSION_DEFINE_KEY(sql_placeholder_name)
QN_FUSION_DEFINE_KEY(classname)
QN_FUSION_DEFINE_KEY(c_classname)
QN_FUSION_DEFINE_KEY(optional)

#define QN_FUSION_PROPERTY_IS_EXTENDED_FOR_base true
#define QN_FUSION_PROPERTY_EXTENSION_FOR_base(KEY, VALUE) (base, std::declval<VALUE>())

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_name true
#define QN_FUSION_PROPERTY_TYPE_FOR_name QString
#define QN_FUSION_PROPERTY_IS_EXTENDED_FOR_name true
#define QN_FUSION_PROPERTY_EXTENSION_FOR_name(KEY, VALUE) (name, QStringLiteral(VALUE))(c_name, VALUE)(sql_placeholder_name, QN_FUSION_LIT_CAT(":", VALUE))

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_c_name true
#define QN_FUSION_PROPERTY_TYPE_FOR_c_name const char *

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_sql_placeholder_name true
#define QN_FUSION_PROPERTY_TYPE_FOR_sql_placeholder_name QString

#define QN_FUSION_PROPERTY_IS_EXTENDED_FOR_setter true
#define QN_FUSION_PROPERTY_EXTENSION_FOR_setter(KEY, VALUE) (setter, VALUE)(setter_tag, QnFusionDetail::make_access_setter_category(access_type())) 

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_classname true
#define QN_FUSION_PROPERTY_TYPE_FOR_classname QString
#define QN_FUSION_PROPERTY_IS_EXTENDED_FOR_classname true
#define QN_FUSION_PROPERTY_EXTENSION_FOR_classname(KEY, VALUE) (classname, QStringLiteral(VALUE))(c_classname, VALUE)

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_c_classname true
#define QN_FUSION_PROPERTY_TYPE_FOR_c_classname const char *

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_optional true
#define QN_FUSION_PROPERTY_TYPE_FOR_optional bool


namespace QnFusion {
    template<class T>
    struct remove_cvr:
        std::remove_cv<
            typename std::remove_reference<T>::type
        >
    {};


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
    T invoke(T Base::*getter, const Derived &object) {
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
    {};

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
            typename Access::template at<setter_type, void>::type,
            typename remove_cvr<decltype(invoke(
                std::declval<typename Access::template at<getter_type, void>::type::result_type>(), 
                std::declval<typename Access::template at<object_declval_type, void>::type::result_type>()
            ))>::type
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
    template<class T, class R>
    struct replace_referent {
        typedef R type;
    };

    template<class T, class R>
    struct replace_referent<T &, R> {
        typedef R &type;
    };

    template<class T, class R>
    struct replace_referent<T &&, R> {
        typedef R &&type;
    };

    template<class T, class R>
    struct replace_referent<const T, R> {
        typedef const R type;
    };

    template<class T, class R>
    struct replace_referent<const T &, R> {
        typedef const R &type;
    };

    template<class T, class R>
    struct replace_referent<const T &&, R> {
        typedef const R &&type;
    };


    template<class Access>
    typename QnFusion::access_setter_category<Access>::type
    make_access_setter_category(const Access &) {
        return typename QnFusion::access_setter_category<Access>::type();
    }


    template<class Base>
    struct NoStartStopVisitorWrapper: Base {
    public:
        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            return Base::operator()(value, access);
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) const {
            return Base::operator()(value, access);
        }
    };

    template<class Visitor>
    NoStartStopVisitorWrapper<Visitor> &no_start_stop_wrap(Visitor &visitor) {
        return static_cast<NoStartStopVisitorWrapper<Visitor> &>(visitor);
    }

    template<class Visitor>
    NoStartStopVisitorWrapper<Visitor> &no_start_stop_wrap(const Visitor &visitor) {
        return static_cast<const NoStartStopVisitorWrapper<Visitor> &>(visitor);
    }


    template<class Visitor, class T, class Access>
    bool dispatch_visit(Visitor &&visitor, T &&value, const Access &access) {
        return dispatch_visit(std::forward<Visitor>(visitor), std::forward<T>(value), access, typename Access::template at<QnFusion::base_type, na>::type());
    }

    template<class Visitor, class T, class Access>
    bool dispatch_visit(Visitor &&visitor, T &&value, const Access &access, const na &) {
        return visitor(std::forward<T>(value), access);
    }

    template<class Visitor, class T, class Access, class Base>
    bool dispatch_visit(Visitor &&visitor, T &&value, const Access &, const Base &) {
        typedef typename replace_referent<T, typename std::remove_reference<typename Base::result_type>::type>::type base_type;

        return QnFusion::visit_members(std::forward<base_type>(value), no_start_stop_wrap(visitor));
    }

} // namespace QnFusionDetail


/**
 * This macro generates several functions for the given fusion-adapted type. 
 * Tokens for the functions to generate are passed in FUNCTION_SEQ parameter. 
 * Accepted tokens are:
 * <ul>
 * <li> <tt>hash</tt>           --- <tt>qHash</tt> function. </li>
 * <li> <tt>datastream</tt>     --- <tt>QDataStream</tt> (de)serialization functions. </li>
 * <li> <tt>eq</tt>             --- <tt>operator==</tt> and <tt>operator!=</tt>. </li>
 * <li> <tt>debug</tt>          --- <tt>QDebug</tt> streaming functions. </li>
 * <li> <tt>json</tt>           --- json (de)serialization functions. </li>
 * <li> <tt>binary</tt>         --- bns (de)serialization functions. </li>
 * <li> <tt>sql</tt>            --- sql bind/fetch functions. </li>
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
    QN_FUSION_DEFINE_FUNCTIONS(BOOST_PP_TUPLE_ENUM(CLASS), BOOST_PP_TUPLE_ENUM(PARAMS))


/**
 * \internal
 * 
 * Produces a <tt>QStringLiteral</tt> from the given character literals.
 * Works around a MSVC bug that prevents us to simply use <tt>QStringLiteral("a" "b")</tt>.
 */
#define QN_FUSION_LIT_CAT(A, B)                                                 \
    QN_FUSION_LIT_CAT_I(A, B)

#ifdef _MSC_VER
#   define QN_FUSION_LIT_CAT_I(A, B) QStringLiteral(A BOOST_PP_CAT(L, B))
#else
#   define QN_FUSION_LIT_CAT_I(A, B) QStringLiteral(A B)
#endif

#endif // QN_FUSION_H
