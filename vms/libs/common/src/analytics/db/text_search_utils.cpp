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
    if (token.endsWith('*'))
        token.truncate(token.size() - 1);
    if (token.empty())
        return;

    m_tokens.push_back(token);
}

//-------------------------------------------------------------------------------------------------
// TextMatcher.

bool TextMatcher::parse(const QString& text)
{
    UserTextSearchExpressionParser parser;
    bool success = false;
    std::tie(success, m_conditions) = parser.parse(text);
    if (!success)
        return false;

    m_conditionsMatched.clear();
    m_conditionsMatched.resize(m_conditions.size(), false);
    return true;
}

bool TextMatcher::empty() const
{
    return m_conditions.empty();
}

void TextMatcher::matchAttributes(const nx::common::metadata::Attributes& attributes)
{
    if (attributes.empty())
        return;

    matchExactAttributes(attributes);
    checkAttributesPresence(attributes);
    matchAttributeValues(attributes);
}

void TextMatcher::matchText(const QString& text)
{
    for (std::size_t i = 0; i < m_conditions.size(); ++i)
    {
        const auto& condition = m_conditions[i];
        if (condition.type != ConditionType::textMatch)
            continue;

        m_conditionsMatched[i] =
            m_conditionsMatched[i] || text.startsWith(condition.text, Qt::CaseInsensitive);
    }
}

bool TextMatcher::matched() const
{
    return std::all_of(
        m_conditionsMatched.begin(), m_conditionsMatched.end(),
        [](auto matched) { return matched; });
}

void TextMatcher::matchExactAttributes(
    const nx::common::metadata::Attributes& attributes)
{
    for (std::size_t i = 0; i < m_conditions.size(); ++i)
    {
        const auto& condition = m_conditions[i];
        if (condition.type != ConditionType::attributeValueMatch)
            continue;

        bool matched = false;
        for (const auto& attr: attributes)
        {
            if (attr.name.startsWith(condition.name, Qt::CaseInsensitive) &&
                attr.value.startsWith(condition.value, Qt::CaseInsensitive))
            {
                matched = true;
            }
        }

        m_conditionsMatched[i] = m_conditionsMatched[i] || matched;
    }
}

void TextMatcher::checkAttributesPresence(
    const nx::common::metadata::Attributes& attributes)
{
    for (std::size_t i = 0; i < m_conditions.size(); ++i)
    {
        const auto& condition = m_conditions[i];
        if (condition.type != ConditionType::attributePresenceCheck)
            continue;

        m_conditionsMatched[i] = m_conditionsMatched[i] ||
            std::any_of(
                attributes.begin(), attributes.end(),
                [&condition](const auto& attr)
                {
                    return attr.name.startsWith(condition.name, Qt::CaseInsensitive);
                });
    }
}

void TextMatcher::matchAttributeValues(
    const nx::common::metadata::Attributes& attributes)
{
    for (std::size_t i = 0; i < m_conditions.size(); ++i)
    {
        const auto& condition = m_conditions[i];
        if (condition.type != ConditionType::textMatch)
            continue;

        m_conditionsMatched[i] =
            m_conditionsMatched[i] || wordMatchAnyOfAttributes(condition.text, attributes);
    }
}

bool TextMatcher::wordMatchAnyOfAttributes(
    const QString& word,
    const nx::common::metadata::Attributes& attributes)
{
    return std::any_of(
        attributes.cbegin(), attributes.cend(),
        [&word](const nx::common::metadata::Attribute& attribute)
        {
            return attribute.value.startsWith(word, Qt::CaseInsensitive);
        });
}

} // namespace nx::analytics::db
