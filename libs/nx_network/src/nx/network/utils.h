// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

#include <nx/ranges.h>
#include <nx/utils/exception.h>

#include "http/abstract_msg_body_source.h"
#include "http/server/request_processing_types.h"

namespace nx::network::utils {

std::optional<std::string> NX_NETWORK_API getHostName(
    const http::RequestContext& request, const std::string& hostHeader = "Host");

template<typename Filter>
concept CookieViewFilterPredicate =
    requires(Filter&& f, std::pair<std::string_view, std::string_view> cookieNameValue)
    {
        { std::invoke(std::forward<Filter>(f), cookieNameValue) } -> std::same_as<bool>;
    };

constexpr nx::Closure cookiesView =
    []<CookieViewFilterPredicate Predicate = nx::traits::AlwaysTrue>(
        const HttpHeaders& headers, Predicate&& filter = {})
            -> nx::ranges::ViewOf<std::pair<std::string_view, std::string_view>> auto
    {
        using namespace std::string_view_literals;
        using namespace nx::ranges;

        static constexpr auto toStringView =
            [](const nx::ToStringViewConvertible auto& s)
            {
                return std::string_view(s);
            };

        const auto cookieHeaders = std::apply(
            [](auto... i)
            {
                return std::ranges::subrange(i...);
            },
            headers.equal_range("cookie"));

        return cookieHeaders
            | std::views::values
            | std::views::transform(
                [filterPredicate = std::forward<Predicate>(filter)](std::string_view v) mutable
                {
                    return v
                        | std::views::split(";"sv)
                        | std::views::transform(toStringView)
                        | std::views::transform(trim)
                        | std::views::filter(
                            [](std::string_view v)
                            {
                                return !v.empty()
                                    // Discarding invalid values, but should be treated as
                                    // `Bad Request` when parsing the request.
                                    && 1 == std::ranges::count(v, '=');
                            })
                        | std::views::transform(
                            [](std::string_view s) -> std::pair<std::string_view, std::string_view>
                            {
                                return s
                                    | std::views::split("="sv)
                                    | std::views::transform(toStringView)
                                    | std::views::transform(trim)
                                    | to_pair;
                            })
                        | std::views::filter(filterPredicate);
                })
            | std::views::join;
    };

} // namespace nx::network::utils
