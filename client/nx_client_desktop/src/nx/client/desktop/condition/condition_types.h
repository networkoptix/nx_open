#pragma once

namespace nx {
namespace client {
namespace desktop {

enum class MatchMode
{
    Any,            //< Match if at least one element satisfies the condition.
    All,            //< Match only if all elements satisfy the condition.
    ExactlyOne      //< Match only if exactly one element satisfies condition.
};

enum class ConditionResult
{
    None,           //< None elements satisfy the condition.
    Partial,        //< Some elements satisfy the condition.
    All             //< All elements satisfy the condition.
};

} // namespace desktop
} // namespace client
} // namespace nx
