#include "log_level.h"

#include "assert.h"
#include "log_message.h"

#include <nx/utils/literal.h>

namespace nx {
namespace utils {
namespace log {

//bool operator<=(Level left, Level right)
//{
//    return static_cast<int>(left) <= static_cast<int>(right);
//}

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
            return QLatin1String("undefined");

        case Level::none:
            return QLatin1String("none");

        case Level::always:
            return QLatin1String("always");

        case Level::error:
            return QLatin1String("error");

        case Level::warning:
            return QLatin1String("warning");

        case Level::info:
            return QLatin1String("info");

        case Level::debug:
            return QLatin1String("debug");

        case Level::verbose:
            return QLatin1String("verbose");

        case Level::notConfigured:
            return QLatin1String("notConfigured");
    };

    NX_ASSERT(false, lm("Unknown level: %1").arg(static_cast<int>(level)));
    return lm("unknown(%1)").arg(static_cast<int>(level));
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
            if (filters.find(t) != filters.end())
                qWarning() << Q_FUNC_INFO << "redefine filter" << t;

            filters.emplace(std::move(t), level);
        }
    }
}

} // namespace log
} // namespace utils
} // namespace nx
