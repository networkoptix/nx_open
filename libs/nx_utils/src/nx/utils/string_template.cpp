#include "string_template.h"

namespace nx::utils {

QString stringTemplate(
    const QString& template_,
    const std::function<QString(const QString& name)> resolve)
{
    QString result;
    result.reserve(template_.size());

    int processed = 0;
    while (processed < template_.size())
    {
        const auto open = template_.indexOf('{', processed);
        if (open == -1)
            break;

        const auto close = template_.indexOf('}', open + 1);
        if (close == -1)
            break; // TODO: throw exception about unpaired brace at open?

        result += template_.midRef(processed, open - processed);
        result += resolve(template_.mid(open + 1, close - open - 1));
        processed = close + 1;
    }

    result += template_.midRef(processed);
    return result;
}

} // namespace nx::utils
