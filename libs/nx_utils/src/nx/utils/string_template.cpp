#include "string_template.h"

#include <QtCore/QRegExp>

namespace nx::utils {

QString stringTemplate(
    const QString& template_,
    const QString& variableMark,
    const std::function<QString(const QString& name)> resolve)
{
    QString result;
    result.reserve(template_.size());

    int processed = 0;
    while (processed < template_.size())
    {
        const auto begin = template_.indexOf(variableMark, processed);
        if (begin == -1)
            break;

        auto end = begin + 1;
        while (end < template_.size() && template_[end].isLetter())
            end++;

        result += template_.midRef(processed, begin - processed);
        if (begin != end)
        {
            const auto name = template_.mid(begin + variableMark.size(), end - begin - 1);
            result += resolve(name);
        }

        processed = end;
    }

    result += template_.midRef(processed);
    return result;
}

} // namespace nx::utils
