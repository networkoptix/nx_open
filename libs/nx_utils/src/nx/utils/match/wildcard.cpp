#include <cctype>

#include "wildcard.h"

bool wildcardMatch(const char* mask, const char* str, MatchMode mode)
{
    while (*str && *mask)
    {
        switch (*mask)
        {
            case '*':
            {
                if (*(mask + 1) == '\0')
                    return true; //< We have '*' at the end.
                for (const char* str1 = str; *str1; ++str1)
                {
                    if (wildcardMatch(mask + 1, str1, mode))
                        return true;
                }
                return false; //< Not matching.
            }

            case '?':
                if (*str == '.')
                    return false;
                break;

            default:
            {
                const bool match = mode == MatchMode::caseSensitive
                    ? *str == *mask
                    : std::tolower(*str) == std::tolower(*mask);

                if (!match)
                    return false;
                break;
            }
        }

        ++str;
        ++mask;
    }

    while (*mask)
    {
        if (*mask != '*')
            return false;
        ++mask;
    }

    if (*str)
        return false;

    return true;
}
