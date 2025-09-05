// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_FUSION_DETAIL_H
#define QN_FUSION_DETAIL_H

#include <type_traits>
#include <utility>

#include "fusion_fwd.h"
#include "fusion_keys.h"

namespace QnFusion {
    template<class T, class Visitor>
    bool visit_members(T &&value, Visitor &&visitor);
} // namespace QnFusion

namespace QnFusionDetail {

    template<class T, class Visitor>
    bool visit_members_internal(T &&value, Visitor &&visitor) {
        return visit_members(std::forward<T>(value), std::forward<Visitor>(visitor)); /* That's the place where ADL kicks in. */
    }

    template<typename... Args, std::invocable<Args...> T>
    constexpr bool safe_operator_call(T&& functor, Args&&... args)
    {
        return functor(std::forward<Args>(args)...);
    }

    template<typename... Args, typename T>
        requires(!std::invocable<T, Args...>)
    constexpr bool safe_operator_call(T&&, Args&&...)
    {
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
    bool dispatch_visit(Visitor &&visitor, T &&value, const Access &access, const std::type_identity<void> &) {
        return visitor(std::forward<T>(value), access);
    }

    template<class Visitor, class T, class Access, class Base>
    bool dispatch_visit(Visitor &&visitor, T &&value, const Access &, const Base &) {
        typedef typename replace_referent<T, typename std::remove_reference<typename Base::result_type>::type>::type base_type;

        return QnFusion::visit_members(std::forward<base_type>(value), no_start_stop_wrap(visitor));
    }

    template<class Visitor, class T, class Access>
    bool dispatch_visit(Visitor &&visitor, T &&value, const Access &access) {
        return dispatch_visit(std::forward<Visitor>(visitor), std::forward<T>(value), access, typename Access::template at<QnFusion::base_type, std::type_identity<void>>::type());
    }

} // namespace QnFusionDetail

#endif // QN_FUSION_DETAIL_H
