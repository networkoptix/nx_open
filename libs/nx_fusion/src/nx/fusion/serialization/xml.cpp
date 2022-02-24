// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "xml.h"

namespace QnXml {

QString replaceProhibitedChars(const QString& s)
{
    // Optimization used: if no prohibited chars found, return the original string without copying.

    bool replacementOccurred = false;
    QString result;
    for (int i = 0; i < s.size(); ++i)
    {
        const QChar c = s.at(i);
        if ((c.unicode() < 0x20 && c.unicode() != 0x9 && c.unicode() != 0xA && c.unicode() != 0xD)
            || c.unicode() == 0xFFFF || c.unicode() == 0xFFFE)
        {
            if (!replacementOccurred) // First-time replacement - copy the previous chars.
            {
                replacementOccurred = true;
                result.reserve(s.size() + 100); //< Optimize potential re-allocations.
                result = s.left(i); //< Copy all the preceding chars (they are valid).
            }
            result.append(QStringLiteral("\\u%1").arg(
                c.unicode(), /*fieldWidth*/ 4, /*base*/ 16, /*fillChar*/ QChar('0')));
        }
        else
        {
            if (replacementOccurred)
                result.append(c);
        }
    }

    if (replacementOccurred)
    {
        result.squeeze();
        return result;
    }
    else
    {
        return s;
    }
}

} // namespace QnXml
