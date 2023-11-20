// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_search_utils.h"

#include <nx/utils/string.h>
#include <regex>

namespace nx::analytics::db {

using namespace nx::common::metadata;
static const int kMinTokenLength = 3;


std::vector<TextSearchCondition> UserTextSearchExpressionParser::parse(const QString& text)
{
    std::vector<TextSearchCondition> result;
    parse(
        text,
        [&result](auto expr) { result.push_back(std::move(expr)); });
    return result;
}

void UserTextSearchExpressionParser::saveToken(QStringView token)
{
    bool isQuoted = false;
    if (token.startsWith('\"'))
    {
        token = token.mid(1);
        isQuoted = true;
    }
    if (token.endsWith('\"') && !token.endsWith(QLatin1String("\\\"")))
    {
        token.truncate(token.size() - 1);
        isQuoted = true;
    }

    if (token.endsWith('*') && !token.endsWith(QLatin1String("\\*")))
        token.truncate(token.size() - 1);
    if (token.empty())
        return;

    m_tokens.push_back({token, isQuoted});
}

QString UserTextSearchExpressionParser::unescape(const QStringView& str)
{
    QString result = str.toString();
    for (int i = 0; i < result.size(); ++i)
    {
        if (result[i] == '\\')
            result.remove(i, 1);
    }

    return result;
}

QString UserTextSearchExpressionParser::unescapeName(const QStringView& str)
{
    QStringView result = str;
    if (result.startsWith('!'))
        result = result.mid(1);
    if (result.startsWith('$'))
        result = result.mid(1);
    return unescape(result);
}

//-------------------------------------------------------------------------------------------------
// TextMatcher.

void TextMatcher::parse(const QString& text)
{
    UserTextSearchExpressionParser parser;
    m_conditions = parser.parse(text);
    m_conditionsMatched.clear();
    m_conditionsMatched.resize(m_conditions.size(), false);
}

bool TextMatcher::hasShortTokens() const
{
    return std::any_of(m_conditions.begin(), m_conditions.end(),
        [](const auto& condition)
        {
            switch (condition.type)
            {
                case ConditionType::numericRangeMatch:
                    return false;
                case ConditionType::attributePresenceCheck:
                    return condition.name.length() < kMinTokenLength;
                case ConditionType::textMatch:
                    return condition.text.length() < kMinTokenLength;
                case ConditionType::attributeValueMatch:
                    return condition.name.length() < kMinTokenLength
                     || condition.value.length() < kMinTokenLength;
                default:
                    return false;
            }
        });
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

        auto comparator =
            [&condition](const auto& attr)
            {
                return attr.name.compare(condition.name, Qt::CaseInsensitive) == 0
                    && attr.value.contains(condition.value, Qt::CaseInsensitive);
            };
        bool matched = condition.isNegative
            ? std::none_of(attributes.begin(), attributes.end(), comparator)
            : std::any_of(attributes.begin(), attributes.end(), comparator);

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

        auto comparator =
            [&condition](const auto& attr)
            {
                return attr.name.compare(condition.name, Qt::CaseInsensitive) == 0
                    || attr.name.startsWith(condition.name + ".", Qt::CaseInsensitive);
            };
        if (condition.isNegative)
        {
            m_conditionsMatched[i] = m_conditionsMatched[i] ||
                std::none_of(attributes.begin(), attributes.end(), comparator);
        }
        else
        {
            m_conditionsMatched[i] = m_conditionsMatched[i] ||
                std::any_of(attributes.begin(), attributes.end(), comparator);
        }
    }
}

void TextMatcher::matchAttributeValues(
    const nx::common::metadata::Attributes& attributes)
{
    for (std::size_t i = 0; i < m_conditions.size(); ++i)
    {
        const auto& condition = m_conditions[i];
        if (condition.type == ConditionType::textMatch)
        {
            if (condition.text.length() < kMinTokenLength)
            {
                // Match the text in the same way as the SQL trigram matcher. It can match >= 3 symbols only.
                continue;
            }
            m_conditionsMatched[i] =
            m_conditionsMatched[i] || wordMatchAnyOfAttributes(condition.text, attributes);
        }
        else if (condition.type == ConditionType::numericRangeMatch)
        {
            m_conditionsMatched[i] =
            m_conditionsMatched[i] || rangeMatchAttributes(condition.range, condition.name, attributes);
        }
    }
}

bool TextMatcher::wordMatchAnyOfAttributes(
    const QString& token,
    const nx::common::metadata::Attributes& attributes)
{
    return std::any_of(
        attributes.cbegin(), attributes.cend(),
        [&token](const nx::common::metadata::Attribute& attribute)
        {
            const auto words = QStringView(attribute.value).split(' ', Qt::SkipEmptyParts);
            return std::any_of(
                words.cbegin(), words.cend(),
                [&token](auto& word) { return word.contains(token, Qt::CaseInsensitive); });
        });
}

bool TextMatcher::rangeMatchAttributes(
    const nx::common::metadata::NumericRange& range,
    const QString& paramName,
    const nx::common::metadata::Attributes& attributes)
{
    return std::any_of(
        attributes.cbegin(), attributes.cend(),
        [&range, &paramName](const nx::common::metadata::Attribute& attribute)
        {
            nx::common::metadata::AttributeEx attributeEx(attribute);
            if (attribute.name.compare(paramName, Qt::CaseInsensitive) != 0)
                return false;
            if (auto value = std::get_if<nx::common::metadata::NumericRange>(&attributeEx.value))
                return value->intersects(range);
            return false;
        });
}

} // namespace nx::analytics::db
