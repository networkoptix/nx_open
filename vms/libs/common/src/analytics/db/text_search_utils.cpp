#include "text_search_utils.h"

#include <nx/utils/string.h>

namespace nx::analytics::db {

std::tuple<bool /*success*/, std::vector<TextSearchCondition>>
    UserTextSearchExpressionParser::parse(const QString& text)
{
    std::vector<TextSearchCondition> result;
    auto success = parse(
        text,
        [&result](auto expr) { result.push_back(std::move(expr)); });
    return std::make_tuple(success, std::move(result));
}

void UserTextSearchExpressionParser::saveToken(QStringView token)
{
    token = nx::utils::unquoteStr(token, '"');
    if (token.empty())
        return;

    m_tokens.push_back(token);
}

} // namespace nx::analytics::db
