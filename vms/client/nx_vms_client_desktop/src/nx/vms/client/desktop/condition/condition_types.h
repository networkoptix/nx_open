// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::desktop {

enum class MatchMode
{
    /** Match if at least one element satisfies the condition. */
    any,

    /** Matches if all elements satisfy the condition. */
    all,

    /* Matches if exactly one element satisfies condition. */
    exactlyOne,

    /** Matches if no elements satisfy the condition. */
    none,
};

enum class ConditionResult
{
    None,           //< None elements satisfy the condition.
    Partial,        //< Some elements satisfy the condition.
    All             //< All elements satisfy the condition.
};

} // namespace nx::vms::client::desktop
