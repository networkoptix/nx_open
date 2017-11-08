#include "log_level.h"

#include <QtCore/QStringBuilder>

#include "assert.h"
#include "log_message.h"

#include <nx/utils/literal.h>

namespace nx {
namespace utils {
namespace log {

Level levelFromString(const QString& levelString)
{
    const auto level = levelString.toLower();
    if (level == lit("none") || level == lit("n"))
        return Level::none;

    if (level == lit("always") || level == lit("a"))
        return Level::always;

    if (level == lit("error") || level == lit("e"))
        return Level::error;

    if (level == lit("warning") || level == lit("w"))
        return Level::warning;

    if (level == lit("info") || level == lit("i"))
        return Level::info;

    if (level == lit("debug") || level == lit("debug1") || level == lit("d"))
        return Level::debug;

    if (level == lit("verbose") || level == lit("debug2") || level == lit("v"))
        return Level::verbose;

    if (level == lit("notconfigured") || level == lit("not_configured"))
        return Level::notConfigured;

    return Level::undefined;
}

QString toString(Level level)
{
    switch (level)
    {
        case Level::undefined:
            return lit("undefined");

        case Level::none:
            return lit("none");

        case Level::always:
            return lit("always");

        case Level::error:
            return lit("error");

        case Level::warning:
            return lit("warning");

        case Level::info:
            return lit("info");

        case Level::debug:
            return lit("debug");

        case Level::verbose:
            return lit("verbose");

        case Level::notConfigured:
            return lit("notConfigured");
    };

    NX_ASSERT(false, lm("Unknown level: %1").arg(static_cast<int>(level)));
    return lm("unknown(%1)").arg(static_cast<int>(level));
}

Tag::Tag(const std::type_info& info):
    m_value(::toString(info))
{
}

Tag::Tag(QString s):
    m_value(std::move(s))
{
}

bool Tag::matches(const Tag& mask) const
{
    // TODO: currently all tags are considered as prefixes, but it might be useful to support
    // some king of regexp in future.
    return m_value.startsWith(mask.m_value);
}

const QString& Tag::toString() const
{
    return m_value;
}

Tag Tag::operator+(const Tag& rhs) const
{
    return Tag(m_value % "::" % rhs.m_value);
}

bool Tag::operator<(const Tag& rhs) const
{
    return m_value < rhs.m_value;
}

bool Tag::operator==(const Tag& rhs) const
{
    return m_value == rhs.m_value;
}

bool Tag::operator!=(const Tag& rhs) const
{
    return m_value != rhs.m_value;
}

LevelSettings::LevelSettings(Level primary, LevelFilters filters):
    primary(primary),
    filters(std::move(filters))
{
}

bool LevelSettings::operator==(const LevelSettings& rhs) const
{
    return primary == rhs.primary && filters == rhs.filters;
}

void LevelSettings::reset()
{
    primary = kDefaultLevel;
    filters.clear();
}

QString LevelSettings::toString() const
{
    return lit("%1, filters=%2").arg(log::toString(primary).toUpper(), containerString(filters));
}

static bool isSeparator(const QChar& c)
{
    return c == QChar(',');
};

static bool isOpenBracket(const QChar& c)
{
    return c == QChar('[');
};

static bool isCloseBracket(const QChar& c)
{
    return c == QChar(']');
};

static bool isTokenPart(const QChar& c)
{
    return !isSeparator(c) && !isOpenBracket(c) && !isCloseBracket(c);
}

static QString readToken(const QString& s, int* position)
{
    int start = *position;
    while (*position < s.size() && isTokenPart(s[*position]))
        ++(*position);

    return s.midRef(start, *position - start).trimmed().toString();
}

static std::set<QString> readTokenList(const QString& s, int* position)
{
    std::set<QString> tokens;
    if (*position == s.size() || !isOpenBracket(s[*position]))
        return tokens;

    ++(*position);
    for (;; ++(*position))
    {
        auto t = readToken(s, position);
        if (!t.isEmpty())
            tokens.insert(std::move(t));

        if (*position == s.size())
            return tokens;

        if (isCloseBracket(s[*position]))
        {
            ++position;
            return tokens;
        }
    }
}

void LevelSettings::parse(const QString& s)
{
    if (s.trimmed().isEmpty())
        return;

    for (int position = 0; position < s.size(); ++position)
    {
        if (isSeparator(s[position]))
            continue;

        const auto levelToken = readToken(s, &position);
        auto level = levelFromString(levelToken);
        auto filtersTokens = readTokenList(s, &position);

        if (level == Level::undefined)
        {
            qWarning() << Q_FUNC_INFO << "ignore wrong level" << levelToken;
            continue;
        }

        if (filtersTokens.empty())
        {
            primary = level;
            continue;
        }

        for (auto& t : filtersTokens)
        {
            Tag tag(std::move(t));
            if (filters.find(tag) != filters.end())
                qWarning() << Q_FUNC_INFO << "redefine filter" << tag.toString();

            filters.emplace(std::move(tag), level);
        }
    }
}

} // namespace log
} // namespace utils
} // namespace nx
