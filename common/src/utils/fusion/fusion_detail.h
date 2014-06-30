#ifndef QN_FUSION_DETAIL_H
#define QN_FUSION_DETAIL_H

#include <utility> /* For std::forward and std::declval. */
#include <type_traits> /* For std::remove_*, std::enable_if, std::integral_constant. */

#ifndef Q_MOC_RUN
#include <boost/mpl/has_xxx.hpp>
#endif 

#include <utils/common/type_traits.h>

#include "fusion_fwd.h"
#include "fusion_keys.h"

namespace QnFusion {
    template<class T, class Visitor>
    bool visit_members(T &&value, Visitor &&visitor);
} // namespace QnFusion

namespace QnFusionDetail {
    using namespace QnTypeTraits;

    template<class T, class Visitor>
    bool visit_members_internal(T &&value, Visitor &&visitor) {
        return visit_members(std::forward<T>(value), std::forward<Visitor>(visitor)); /* That's the place where ADL kicks in. */
    }


    BOOST_MPL_HAS_XXX_TRAIT_DEF(type)


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
    bool dispatch_visit(Visitor &&visitor, T &&value, const Access &access, const na &) {
        return visitor(std::forward<T>(value), access);
    }

    template<class Visitor, class T, class Access, class Base>
    bool dispatch_visit(Visitor &&visitor, T &&value, const Access &, const Base &) {
        typedef typename replace_referent<T, typename std::remove_reference<typename Base::result_type>::type>::type base_type;

        return QnFusion::visit_members(std::forward<base_type>(value), no_start_stop_wrap(visitor));
    }

    template<class Visitor, class T, class Access>
    bool dispatch_visit(Visitor &&visitor, T &&value, const Access &access) {
        return dispatch_visit(std::forward<Visitor>(visitor), std::forward<T>(value), access, typename Access::template at<QnFusion::base_type, na>::type());
    }

} // namespace QnFusionDetail

#endif // QN_FUSION_DETAIL_H
