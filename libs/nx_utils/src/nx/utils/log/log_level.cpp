#include "log_level.h"

#include <QtCore/QStringBuilder>

#include "assert.h"
#include "log_message.h"
#include "log_main.h"

namespace nx {
namespace utils {
namespace log {

namespace {

QRegularExpression makeExactMatchPattern(const Tag& tag)
{
    return QRegularExpression("^" + tag.toString() + "$");
}

} // namespace

Level levelFromString(const QString& levelString)
{
    const auto level = levelString.toLower();
    if (level == "none" || level == "n")
        return Level::none;

    if (level == "always" || level == "a")
        return Level::always;

    if (level == "error" || level == "e")
        return Level::error;

    if (level == "warning" || level == "w")
        return Level::warning;

    if (level == "info" || level == "i")
        return Level::info;

    if (level == "debug" || level == "debug1" || level == "d")
        return Level::debug;

    if (level == "verbose" || level == "debug2" || level == "v")
        return Level::verbose;

    if (level == "notconfigured" || level == "not_configured")
        return Level::notConfigured;

    return Level::undefined;
}

QString toString(Level level)
{
    switch (level)
    {
        case Level::undefined:
            return "undefined";

        case Level::none:
            return "none";

        case Level::always:
            return "always";

        case Level::error:
            return "error";

        case Level::warning:
            return "warning";

        case Level::info:
            return "info";

        case Level::debug:
            return "debug";

        case Level::verbose:
            return "verbose";

        case Level::notConfigured:
            return "notConfigured";
    };

    NX_ASSERT(false, lm("Unknown level: %1").arg(static_cast<int>(level)));
    return lm("unknown(%1)").arg(static_cast<int>(level));
}

//-------------------------------------------------------------------------------------------------
// Tag

Tag::Tag(const std::type_info& info):
    m_value(::toString(info))
{
}

Tag::Tag(QString s):
    m_value(std::move(s))
{
}

Tag::Tag(const std::string& s):
    m_value(QString::fromStdString(s))
{
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

//-------------------------------------------------------------------------------------------------
// Filter

Filter::Filter(const QRegularExpression& source):
    m_filter(source),
    m_valid(m_filter.isValid())
{
    if (m_valid)
        m_filter.optimize();
}

Filter::Filter(const QString& source):
    Filter(QRegularExpression(source,
		QRegularExpression::CaseInsensitiveOption | QRegularExpression::OptimizeOnFirstUsageOption))
{
}

Filter::Filter(const Tag& tag):
    Filter(makeExactMatchPattern(tag))
{
}

bool Filter::isValid() const
{
    return m_filter.isValid();
}

bool Filter::accepts(const Tag& tag) const
{
    if (!isValid())
        return false;

    return m_filter.match(tag.toString()).hasMatch();
}

QString Filter::toString() const
{
    return m_filter.pattern();
}

bool Filter::operator<(const Filter& rhs) const
{
    return toString() < rhs.toString();
}

bool Filter::operator==(const Filter& rhs) const
{
    return toString() == rhs.toString();
}

bool Filter::operator!=(const Filter& rhs) const
{
    return !(*this == rhs);
}

//-------------------------------------------------------------------------------------------------
// LevelSettings

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

const QChar kSeparator(',');
const QChar kOpenBracket('[');
const QChar kCloseBracket(']');

QString LevelSettings::toString() const
{
    std::map<nx::utils::log::Level, QStringList> filersByLevel;
    for (const auto& tag: filters)
        filersByLevel[tag.second] << tag.first.toString();

    QStringList levelList(log::toString(primary));
    for (const auto& filterGroup : filersByLevel)
    {
        const auto level = log::toString(filterGroup.first);
        const auto filters = filterGroup.second.join(kSeparator);
        levelList << (level + kOpenBracket + filters + kCloseBracket);
    }

    return levelList.join(kSeparator + QString(" "));
}

static bool isSeparator(const QChar& c)
{
    return c == kSeparator;
};

static bool isOpenBracket(const QChar& c)
{
    return c == kOpenBracket;
};

static bool isCloseBracket(const QChar& c)
{
    return c == kCloseBracket;
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

bool LevelSettings::parse(const QString& s)
{
    if (s.trimmed().isEmpty())
        return true; //< Not an error, default value will be used.

    bool hasErrors = false;
    for (int position = 0; position < s.size(); ++position)
    {
        if (isSeparator(s[position]))
            continue;

        const auto levelToken = readToken(s, &position);
        auto filtersTokens = readTokenList(s, &position);

        auto level = levelFromString(levelToken);
        if (level == Level::undefined)
        {
            qWarning() << Q_FUNC_INFO << "ignore wrong level" << levelToken;
            hasErrors = true;
            continue;
        }

        if (filtersTokens.empty())
        {
            primary = level;
            continue;
        }

        for (auto& t: filtersTokens)
        {
            Filter filter(t);
            if (filters.find(filter) != filters.end())
            {
                qWarning() << Q_FUNC_INFO << "redefine filter" << filter.toString();
                hasErrors = true;
            }

            filters.emplace(std::move(filter), level);
        }
    }

    return !hasErrors;
}

} // namespace log
} // namespace utils
} // namespace nx
