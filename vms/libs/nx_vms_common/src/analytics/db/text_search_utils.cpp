// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_search_utils.h"

#include <regex>

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

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
    const bool matchesFromStart = token.startsWith('^');
    if (matchesFromStart)
        token = token.mid(1);
    const bool matchesTillEnd = token.endsWith('$');
    if (matchesTillEnd)
        token.chop(1);

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

    m_tokens.push_back({
        .value = token.toString(),
        .matchesFromStart = matchesFromStart,
        .matchesTillEnd = matchesTillEnd,
        .isQuoted = isQuoted});
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

TextMatcher::TextMatcher(const QString& text)
{
    parse(text);
}

void TextMatcher::parse(const QString& text)
{
    UserTextSearchExpressionParser parser;
    m_conditions = parser.parse(text);
    if (m_conditions.size() > 64)
    {
        NX_WARNING(this, "Text matcher supports up to 64 condittions. %1 provided", m_conditions.size());
        m_conditions.erase(m_conditions.begin() + 64, m_conditions.end());
    }
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
                     || condition.valueToken.value.length() < kMinTokenLength;
                default:
                    return false;
            }
        });
}

bool TextMatcher::empty() const
{
    return m_conditions.empty();
}

bool TextMatcher::matchAttributes(const nx::common::metadata::Attributes& attributes) const
{
    if (attributes.empty())
        return false;

    const uint64_t mask = matchExactAttributes(attributes)
        | checkAttributesPresence(attributes)
        | matchAttributeValues(attributes);
    return allConditionMatched(mask);
}

bool TextMatcher::matchText(const QString& text) const
{
    uint64_t result = 0;
    for (std::size_t i = 0; i < m_conditions.size(); ++i)
    {
        const auto& condition = m_conditions[i];
        if (condition.type != ConditionType::textMatch)
            continue;
        if (text.startsWith(condition.text, Qt::CaseInsensitive))
            result |= (1 << i);
    }
    return allConditionMatched(result);
}

bool TextMatcher::allConditionMatched(uint64_t result) const
{
    const uint64_t mask = (1ULL << m_conditions.size()) - 1;
    return result == mask;
}

bool TextMatcher::isAttributeNameMatching(const QString& attributeName, const QString& value)
{
    if (value.endsWith('*'))
        return attributeName.startsWith(value.chopped(1), Qt::CaseInsensitive);

    return attributeName.compare(value, Qt::CaseInsensitive) == 0
        // NOTE: The period is used to separate Attribute names of nested Objects.
        || attributeName.startsWith(value + ".", Qt::CaseInsensitive);
}

uint64_t TextMatcher::matchExactAttributes(
    const nx::common::metadata::Attributes& attributes) const
{
    uint64_t result = 0;
    for (std::size_t i = 0; i < m_conditions.size(); ++i)
    {
        const auto& condition = m_conditions[i];
        if (condition.type != ConditionType::attributeValueMatch)
            continue;

        auto comparator =
            [&condition](const auto& attr)
        {
                if (!isAttributeNameMatching(attr.name, condition.name))
                    return false;

                if (condition.valueToken.matchesFromStart && condition.valueToken.matchesTillEnd)
                {
                    return attr.value.compare(condition.valueToken.value, Qt::CaseInsensitive) == 0;
                }
                if (condition.valueToken.matchesFromStart)
                    return attr.value.startsWith(condition.valueToken.value, Qt::CaseInsensitive);
                if (condition.valueToken.matchesTillEnd)
                    return attr.value.endsWith(condition.valueToken.value, Qt::CaseInsensitive);
                return attr.value.contains(condition.valueToken.value, Qt::CaseInsensitive);
        };

        const bool isMatched = condition.isNegative
            ? std::none_of(attributes.begin(), attributes.end(), comparator)
            : std::any_of(attributes.begin(), attributes.end(), comparator);
        if (isMatched)
            result |= (1 << i);
    }
    return result;
}

uint64_t TextMatcher::checkAttributesPresence(
    const nx::common::metadata::Attributes& attributes) const
{
    uint64_t result = 0;
    for (std::size_t i = 0; i < m_conditions.size(); ++i)
    {
        const auto& condition = m_conditions[i];
        if (condition.type != ConditionType::attributePresenceCheck)
            continue;

        auto comparator =
            [&condition](const auto& attr)
            {
                return isAttributeNameMatching(attr.name, condition.name);
            };
        const bool isMatched = condition.isNegative
            ? std::none_of(attributes.begin(), attributes.end(), comparator)
            : std::any_of(attributes.begin(), attributes.end(), comparator);
        if (isMatched)
            result |= (1 << i);
    }
    return result;
}

uint64_t TextMatcher::matchAttributeValues(
    const nx::common::metadata::Attributes& attributes) const
{
    uint64_t result = 0;
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
            if (wordMatchAnyOfAttributes(condition.text, attributes))
                result |= (1 << i);
        }
        else if (condition.type == ConditionType::numericRangeMatch)
        {
            if (rangeMatchAttributes(condition.range, condition.name, attributes))
                result |= (1 << i);
        }
    }
    return result;
}

bool TextMatcher::wordMatchAnyOfAttributes(
    const QString& token,
    const nx::common::metadata::Attributes& attributes) const
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
    const nx::common::metadata::Attributes& attributes) const
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

QString serializeTextSearchConditions(const std::vector<TextSearchCondition>& conditions)
{
    auto quoteIfNeeded =
        [](const QString& str) { return str.contains(" ") ? '"' + str + '"' : str; };

    auto serialize =
        [&](const TextSearchCondition& condition) -> QString
        {
            switch (condition.type)
            {
                case ConditionType::attributePresenceCheck:
                    return (condition.isNegative ? "!$" : "$") + quoteIfNeeded(condition.name);

                case ConditionType::attributeValueMatch:
                {
                    return quoteIfNeeded(condition.name)
                        + (condition.isNegative ? "!=" : "=")
                        + (condition.valueToken.matchesFromStart ? "^" : "")
                        + quoteIfNeeded(condition.valueToken.value)
                        + (condition.valueToken.matchesTillEnd ? "$" : "");
                }

                case ConditionType::textMatch:
                    return condition.text;

                case ConditionType::numericRangeMatch:
                {
                    return quoteIfNeeded(condition.name)
                        + "="
                        + condition.range.stringValue().remove(" ");
                }
            }

            NX_ASSERT(false, "Unexpected condition type (%1)", static_cast<int>(condition.type));
            return "";
        };

    QStringList result;
    for (const auto& condition: conditions)
        result.append(serialize(condition));

    return result.join(" ");
}

} // namespace nx::analytics::db
