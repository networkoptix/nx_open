#pragma once

#include <QString>

/**
 * Check whether a string matches a wildcard expression.
 *
 * @param str String being validated to match the wildcard expression.
 * @param mask Wildcard expression. Allowed to contain special chars '?' (matches any char except
 * '.') and '*' (matches any number of any chars).
 * @return Whether str matches mask.
 */
NX_UTILS_API bool wildcardMatch(const char* mask, const char* str);

inline bool wildcardMatch(const QString& mask, const QString& str)
{
    return wildcardMatch(mask.toLatin1().constData(), str.toLatin1().constData());
}
