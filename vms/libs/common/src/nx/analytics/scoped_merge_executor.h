#pragma once

namespace nx::analytics {

template<typename Descriptor>
class ScopedMergeExecutor
{
public:
    std::optional<Descriptor> operator()(const Descriptor* first, const Descriptor* second) const
    {
        if (first)
        {
            if (!second)
                return *first;

            auto result = *second;
            result.scopes.insert(first->scopes.begin(), first->scopes.end());
            return result;
        }

        if (!second)
            return std::nullopt;

        return *second;
    }
};

} // namespace nx::analytics
