// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "string_template.h"

namespace nx::utils {

bool isIdentifier(const QChar& letter)
{
    return letter.isLetter() || letter.isDigit() || letter == '_';
}

QString stringTemplate(
    const QString& template_,
    const QString& variableMark,
    const std::function<QString(const QString& name)> resolve)
{
    QString result;
    result.reserve(template_.size());
    const QStringView templateView(template_);

    int processed = 0;
    while (processed < template_.size())
    {
        const auto begin = template_.indexOf(variableMark, processed);
        if (begin == -1)
            break;

        auto end = begin + 1;
        while (end < template_.size() && isIdentifier(template_[end]))
            end++;

        result += templateView.mid(processed, begin - processed);
        if (begin != end)
        {
            const auto name = template_.mid(begin + variableMark.size(), end - begin - 1);
            result += resolve(name);
        }

        processed = end;
    }

    result += templateView.mid(processed);
    return result;
}

} // namespace nx::utils
