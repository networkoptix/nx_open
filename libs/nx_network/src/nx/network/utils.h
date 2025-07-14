// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>
#include <nx/utils/std/ranges.h>
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

constexpr nx::utils::ranges::Closure cookiesView =
    []<CookieViewFilterPredicate Predicate = nx::utils::ranges::AlwaysTrue>(
        const HttpHeaders& headers, Predicate&& filter = {})
            -> nx::utils::ranges::ViewOf<std::pair<std::string_view, std::string_view>> auto
    {
        using namespace nx::utils::ranges;

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
                        | split(";")
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
                                    | split("=")
                                    | std::views::transform(trim)
                                    | to_pair
                                    ;
                            })
                        | std::views::filter(filterPredicate)
                        ;
                })
            | std::views::join
            ;
    };

} // namespace nx::network::utils
