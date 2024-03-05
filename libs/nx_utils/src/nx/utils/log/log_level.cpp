// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_level.h"

#include <regex>

#include <QtCore/QStringBuilder>

#include "assert.h"
#include "format.h"
#include "log_main.h"

namespace nx::log {

Level levelFromString(const QString& levelString)
{
    const auto level = levelString.toLower();
    if (level == "none" || level == "n")
        return Level::none;

    if (level == "error" || level == "e")
        return Level::error;

    if (level == "warning" || level == "w")
        return Level::warning;

    if (level == "info" || level == "always" || level == "i")
        return Level::info;

    if (level == "debug" || level == "debug1" || level == "d")
        return Level::debug;

    if (level == "verbose" || level == "debug2" || level == "v")
        return Level::verbose;

    if (level == "trace" || level == "t")
        return Level::trace;

    if (level == "notconfigured" || level == "not_configured")
        return Level::notConfigured;

    return Level::undefined;
}

static const std::array<QString, kLevelsCount> kLevelStrings =
    nx::reflect::enumeration::visitAllItems<Level>(
        [](auto&&... items)
        {
            return std::array<QString, kLevelsCount>{
                QString::fromStdString(nx::reflect::enumeration::toString(items.value))...};
        });

template<typename... Items, size_t... N>
static size_t findIndex(Level level, std::index_sequence<N...>, Items&&... items)
{
    size_t index;
    bool found = false;
    found = ((items.value == level ? (found = true, index = N, found) : found) || ... || false);
    if (!found)
        return sizeof...(items);
    return index;
}

QString toString(Level level)
{
    const auto idx = nx::reflect::enumeration::visitAllItems<Level>(
        [level](auto&&... items)
        {
            return findIndex(level, std::make_index_sequence<sizeof...(items)>(), items...);
        });
    const auto id = static_cast<int>(level);
    if (!NX_ASSERT(idx < kLevelStrings.size(), "Unknown level: %1", id))
        return NX_FMT("unknown(%1)", id);
    return kLevelStrings[idx];
}

//-------------------------------------------------------------------------------------------------
// Tag

Tag::Tag(const std::type_info& info):
    m_value(nx::toString(info))
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

class Filter::Impl
{
public:
    QString pattern;
    std::optional<std::regex> regex;
    bool valid = true;

    Impl(const QString& pattern):
        pattern(pattern)
    {
    }
};

Filter::Filter(const QString& source):
    m_impl(std::make_shared<Impl>(source))
{
    // Trying to parse pattern as a regular expression.
    static const QString kRegularExpressionPattern("re:");
    if (m_impl->pattern.startsWith(kRegularExpressionPattern))
    {
        try
        {
            const QString pattern(m_impl->pattern.mid(kRegularExpressionPattern.length()));
            std::regex re(pattern.toStdString(),
                std::regex_constants::ECMAScript | std::regex_constants::icase);
            m_impl->regex = re;
            m_impl->valid = true;
        }
        catch (std::regex_error&)
        {
            // Syntax error in the regular expression.
            m_impl->valid = false;
        }
    }
    else
    {
        m_impl->valid = !m_impl->pattern.isEmpty();
    }
}

Filter::Filter(const std::string& source):
    Filter(QString::fromStdString(source))
{
}

Filter::Filter(const Tag& tag):
    Filter(tag.toString())
{
}

Filter::~Filter()
{
}

bool Filter::isValid() const
{
    return m_impl->valid;
}

bool Filter::accepts(const Tag& tag) const
{
    if (!isValid())
        return false;

    if (m_impl->regex)
        return std::regex_search(tag.toString().toStdString(), *m_impl->regex);

    return tag.toString().startsWith(m_impl->pattern);
}

QString Filter::toString() const
{
    return m_impl->pattern;
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
    std::map<nx::log::Level, QStringList> filersByLevel;
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

static QString readToken(const QStringView& s, int* position)
{
    int start = *position;
    while (*position < s.size() && isTokenPart(s[*position]))
        ++(*position);

    return s.mid(start, *position - start).trimmed().toString();
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

} // namespace nx::log
