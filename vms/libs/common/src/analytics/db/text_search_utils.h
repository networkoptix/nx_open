#pragma once

#include <optional>
#include <vector>

#include <QtCore/QString>

#include <nx/utils/scope_guard.h>

#include <analytics/common/object_metadata.h>

namespace nx::analytics::db {

inline bool isKeyValueDelimiter(QChar ch) { return ch == ':' || ch == '='; }
inline bool isKeyValueDelimiter(const QStringView& value)
{
    return value.size() == 1 && isKeyValueDelimiter(value[0]);
}

enum class ConditionType
{
    attributePresenceCheck,
    attributeValueMatch,
    textMatch,
};

struct TextSearchCondition
{
    const ConditionType type;

    QString name;
    QString value;
    QString text;

    TextSearchCondition(ConditionType type): type(type) {}

    bool operator==(const TextSearchCondition& right) const
    {
        return type == right.type
            && name == right.name
            && value == right.value
            && text == right.text;
    }
};

struct AttributePresenceCheck:
    public TextSearchCondition
{
    AttributePresenceCheck(const QString& name):
        TextSearchCondition(ConditionType::attributePresenceCheck)
    {
        this->name = name;
    }
};

struct AttributeValueMatch:
    public TextSearchCondition
{
    AttributeValueMatch(const QString& name, const QString& value):
        TextSearchCondition(ConditionType::attributeValueMatch)
    {
        this->name = name;
        this->value = value;
    }
};

struct TextMatch:
    public TextSearchCondition
{
    TextMatch(const QString& text):
        TextSearchCondition(ConditionType::textMatch)
    {
        this->text = text;
    }
};

/**
 * Format description:
 * TextFilter = *matchToken *[ SP matchToken ]
 * matchToken = textExpr | attributePresenceExpr | attributeMatchToken
 * textExpr = ['"'] TEXT ['"']
 * attributePresenceExpr = "$" TEXT
 * attributeMatchToken = TEXT ":" ['"'] TEXT ['"']
 */
class UserTextSearchExpressionParser
{
public:
    /**
     * Found user expressions are reported to handler.
     * @param text The format is specified in the class description.
     * @return true if text is valid.
     */
    template<typename Handler>
    // requires std::is_invocable_v<Handler, TextSearchCondition>
    void parse(const QString& text, Handler handler);

    std::vector<TextSearchCondition> parse(const QString& text);

private:
    void saveToken(QStringView token);

    template<typename Handler>
    void processTokens(Handler& handler);

    /**
     * Replaces \CHAR with CHAR. Does NOT replace \n with LF or similar.
     */
    QString unescape(const QStringView& str);

private:
    std::vector<QStringView> m_tokens;
};

//-------------------------------------------------------------------------------------------------

/**
 * Matches given attributes against search expression defined by UserTextSearchExpressionParser.
 */
class TextMatcher
{
public:
    /**
     * Uses UserTextSearchExpressionParser to parse text.
     */
    void parse(const QString& text);
    bool empty() const;

    void matchAttributes(const nx::common::metadata::Attributes& attributes);
    void matchText(const QString& text);

    /**
     * @return true If all tokens of the filter were matched
     * by calls to matchAttributes or matchText().
     */
    bool matched() const;

private:
    void matchExactAttributes(
        const nx::common::metadata::Attributes& attributes);

    void checkAttributesPresence(
        const nx::common::metadata::Attributes& attributes);

    void matchAttributeValues(
        const nx::common::metadata::Attributes& attributes);

    bool wordMatchAnyOfAttributes(
        const QString& word,
        const nx::common::metadata::Attributes& attributes);

private:
    std::vector<TextSearchCondition> m_conditions;
    std::vector<bool> m_conditionsMatched;
};

//-------------------------------------------------------------------------------------------------

template<typename Handler>
void UserTextSearchExpressionParser::parse(const QString& userText, Handler handler)
{
    enum class State
    {
        waitingTokenStart,
        readingToken,
    };

    QString text = userText;

    bool quoted = false;
    int tokenStart = 0;
    m_tokens.clear();
    State state = State::waitingTokenStart;

    for (int i = 0; i < text.size(); ++i)
    {
        const QChar ch = text[i];

        switch (state)
        {
            case State::waitingTokenStart:
                if (ch.isSpace())
                    continue;
                tokenStart = i;
                state = State::readingToken;
                --i; // Processing the current char again.
                break;

            case State::readingToken:
                if (ch == '\\')
                {
                    // Not processing the current character.
                    ++i;
                    continue;
                }

                if (ch == '"')
                    quoted = !quoted;
                if (quoted)
                    continue;

                if (ch.isSpace() || isKeyValueDelimiter(ch))
                {
                    saveToken(text.midRef(tokenStart, i - tokenStart));
                    if (isKeyValueDelimiter(ch))
                        saveToken(text.midRef(i, 1)); //< Adding key/value delimiter as a separate token.
                    state = State::waitingTokenStart;
                }
                break;
        }
    }

    if (state == State::readingToken)
        saveToken(text.midRef(tokenStart));

    processTokens(handler);
}

template<typename Handler>
void UserTextSearchExpressionParser::processTokens(Handler& handler)
{
    int lastProcessedTokenIndex = -1;
    for (int i = 0; i < (int) m_tokens.size(); ++i)
    {
        const auto& token = m_tokens[i];
        const std::optional<QStringView> nextToken =
            (i+1) < (int) m_tokens.size() ? std::make_optional(m_tokens[i+1]) : std::nullopt;
        const std::optional<QStringView> previousToken =
            (i-1) > lastProcessedTokenIndex ? std::make_optional(m_tokens[i-1]) : std::nullopt;

        auto lastProcessedTokenIndexUpdater = nx::utils::makeScopeGuard(
            [&lastProcessedTokenIndex, &i]() { lastProcessedTokenIndex = i; });

        if (token.startsWith('$'))
        {
            handler(AttributePresenceCheck(unescape(token.mid(1))));
        }
        else if (isKeyValueDelimiter(token))
        {
            if (!previousToken)
                continue; //< Ignoring misplaced key/value delimiter.

            if (!nextToken)
            {
                // Treating "name:" similar to "$name"
                handler(AttributePresenceCheck(unescape(*previousToken)));
                continue;
            }

            handler(AttributeValueMatch(unescape(*previousToken), unescape(*nextToken)));
            // We have already consumed the next token.
            ++i;
        }
        else if (nextToken && isKeyValueDelimiter(*nextToken))
        {
            // Skipping token: it will be processed on the next iteration.
            lastProcessedTokenIndexUpdater.disarm();
        }
        else
        {
            handler(TextMatch(unescape(token)));
        }
    }
}

} // namespace nx::analytics::db::test
