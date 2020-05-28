#pragma once

#include <optional>
#include <tuple>
#include <vector>

#include <QtCore/QString>

namespace nx::analytics::db {

enum class ConditionType
{
    none,
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
    bool parse(const QString& text, Handler handler);

    std::tuple<bool /*success*/, std::vector<TextSearchCondition>> parse(const QString& text);

private:
    void saveToken(QStringView token);

    template<typename Handler>
    bool processTokens(Handler& handler);

private:
    std::vector<QStringView> m_tokens;
};

//-------------------------------------------------------------------------------------------------

template<typename Handler>
bool UserTextSearchExpressionParser::parse(const QString& text, Handler handler)
{
    enum class State
    {
        waitingTokenStart,
        readingToken,
    };

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
                if (ch == '"')
                    quoted = !quoted;
                if (quoted)
                    continue;

                if (ch.isSpace())
                {
                    saveToken(text.midRef(tokenStart, i - tokenStart));
                    state = State::waitingTokenStart;
                }
                else if (ch == ':')
                {
                    // Finishing token just before ':'.
                    saveToken(text.midRef(tokenStart, i - tokenStart));
                    // Adding ':' as a separate token.
                    saveToken(text.midRef(i, 1));
                    state = State::waitingTokenStart;
                }
                break;
        }
    }

    if (state == State::readingToken)
        saveToken(text.midRef(tokenStart));

    return processTokens(handler);
}

template<typename Handler>
bool UserTextSearchExpressionParser::processTokens(Handler& handler)
{
    for (int i = 0; i < m_tokens.size(); ++i)
    {
        const auto& token = m_tokens[i];
        const std::optional<QStringView> nextToken =
            (i+1) < m_tokens.size() ? std::make_optional(m_tokens[i+1]) : std::nullopt;
        const std::optional<QStringView> previousToken =
            i > 0 ? std::make_optional(m_tokens[i - 1]) : std::nullopt;

        if (token.startsWith('$'))
        {
            handler(AttributePresenceCheck(token.mid(1).toString()));
        }
        else if (token == ':')
        {
            // There MUST be tokens on both sides of ':'.
            if (!previousToken || !nextToken)
                return false;
            handler(AttributeValueMatch(previousToken->toString(), nextToken->toString()));
            // We have already consumed the next token.
            ++i;
        }
        else if (nextToken && nextToken == ':')
        {
            // Skipping token: it will be processed on the next iteration.
        }
        else
        {
            handler(TextMatch(token.toString()));
        }
    }

    return true;
}

} // namespace nx::analytics::db::test
