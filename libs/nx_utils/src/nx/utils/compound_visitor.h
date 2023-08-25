// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::utils {

/**
 * Composes multiple functors into one, merging their `operator()`s into a single overload set.
 * The primary purpose is to compose lambdas for variant visitation:
 *
 * <pre><code>
 * struct A {};
 * struct B {};
 * struct C {};
 *
 * using T = std::variant<A, B, C>;
 *
 * T getT();
 *
 * void f()
 * {
 *     std::visit(
 *         CompoundVisitor{
 *             [&](const A&) { std::cout << "A\n"; },
 *             [&](const B&) { std::cout << "B\n"; },
 *             [&](const C&) { std::cout << "C\n"; },
 *         },
 *         getT());
 * }
 * </code></pre>
 */
template <typename... Functors>
struct CompoundVisitor: Functors...
{
    using Functors::operator()...;
};

template <typename... Functors>
CompoundVisitor(Functors...) -> CompoundVisitor<Functors...>;

} // namespace nx::utils
