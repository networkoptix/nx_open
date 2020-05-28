#include "text_search_utils.h"

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
    if (token.empty())
        return;

    if (token[0] == '"')
        token = token.mid(1);
    if (token.empty())
        return;

    if (token.back() == '"')
        token = token.mid(0, token.size() - 1);

    m_tokens.push_back(token);
}

} // namespace nx::analytics::db
