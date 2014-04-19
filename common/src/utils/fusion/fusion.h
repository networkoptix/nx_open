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

    template<class T, class Arg>
    boost::type_traits::yes_type has_operator_call_test(const T &, const Arg &, const decltype(std::declval<T>()(std::declval<Arg>())) * = NULL);
    boost::type_traits::no_type has_operator_call_test(...);

    template<class T, class Arg>
    struct has_operator_call:
        boost::mpl::equal_to<
            boost::mpl::sizeof_<boost::type_traits::yes_type>,
            boost::mpl::size_t<sizeof(has_operator_call_test(std::declval<T>(), std::declval<Arg>()))>
        >
    {};

    template<class T, class Arg>
    bool safe_operator_call(T &&visitor, Arg &&value, const typename boost::enable_if<has_operator_call<T, Arg> >::type * = NULL) {
        return visitor(std::forward<Arg>(value));
    }

    template<class T, class Arg>
    bool safe_operator_call(T &&, Arg &&, const typename boost::disable_if<has_operator_call<T, Arg> >::type * = NULL) {
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

QN_FUSION_DEFINE_KEY(index)
QN_FUSION_DEFINE_KEY(object_declval)
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

    struct start_tag {};
    struct end_tag {};

    struct member_setter_tag {};
    struct function_setter_tag {};

    template<class T>
    struct typed_member_setter_tag: member_setter_tag {};

    template<class T>
    struct typed_function_setter_tag: function_setter_tag {};

    template<class Setter, class T>
    struct setter_category: 
        boost::mpl::if_<
            std::is_member_object_pointer<Setter>,
            typed_member_setter_tag<T>,
            typed_function_setter_tag<T>
        >
    {};

    /**
     * Extension interface that defines how common keys are handled.
     */
    template<class Base>
    struct AccessExtension: Base {
    public:
        using Base::operator();

        template<class Sig>
        struct result: 
            Base::template result<Sig>
        {};

        template<class F>
        struct result<F(setter_tag_type)>:
            setter_category<
                typename result<F(setter_type)>::type,
                typename remove_cvr<decltype(invoke(std::declval<Base>()(getter), std::declval<Base>()(object_declval)))>::type
            >
        {};

        typename result<void(setter_tag_type)>::type operator()(const setter_tag_type &) const {
            return result<void(setter_tag_type)>::type();
        }
    };

    /**
     * This class is the external interface that is to be used by visitor
     * classes.
     */
    template<class Base>
    struct AccessAdaptor {
    public:
        template<class Key>
        decltype(Base()(Key())) operator()(const Key &) const {
            return Base()(Key());
        }

        template<class Key, class T>
        decltype(Base()(Key())) operator()(const Key &, const T &, const typename boost::enable_if<QnFusionDetail::has_operator_call<Base, Key> >::type * = NULL) const {
            return Base()(Key());
        }

        template<class Key, class T>
        T operator()(const Key &, const T &defaultValue, const typename boost::disable_if<QnFusionDetail::has_operator_call<Base, Key> >::type * = NULL) const {
            return defaultValue;
        }
    };

    template<class T>
    struct has_visit_members: 
        QnFusionDetail::has_visit_members<T>
    {};

} // namespace QnFusion


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
