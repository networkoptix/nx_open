#pragma once

#include <map>
#include <set>

#include <nx/utils/log/to_string.h>

namespace nx {
namespace utils {
namespace log {

enum class Level
{
    undefined,
    none,
    always,
    error,
    warning,
    info,
    debug,
    verbose,
    notConfigured = 0xFF
};

Level NX_UTILS_API levelFromString(const QString& levelString);
QString NX_UTILS_API toString(Level level);

#ifdef _DEBUG
    static constexpr Level kDefaultLevel = Level::debug;
#else
    static constexpr Level kDefaultLevel = Level::info;
#endif

class NX_UTILS_API Tag
{
public:
    Tag() = default;
    Tag(const char* s) = delete;
    Tag(const std::type_info& info);
    explicit Tag(QString s);
    explicit Tag(const std::string& s);

    template<typename T>
    Tag(const T* pointer): m_value(::toString(pointer)) {}

    template<typename T>
    Tag join(const T* pointer) const { return Tag(m_value + "::" + ::toString(pointer)); }

    bool matches(const Tag& mask) const;
    const QString& toString() const;
    Tag operator+(const Tag& rhs) const;

    bool operator<(const Tag& rhs) const;
    bool operator==(const Tag& rhs) const;
    bool operator!=(const Tag& rhs) const;

private:
    QString m_value;
};

using LevelFilters = std::map<Tag, Level>;

struct NX_UTILS_API LevelSettings
{
    Level primary = kDefaultLevel;
    LevelFilters filters;

    LevelSettings(Level primary = kDefaultLevel, LevelFilters filters = {});
    bool operator==(const LevelSettings& rhs) const;

    void reset();
    QString toString() const;

    /**
     * Parses settings according to the grammar:
     * <pre><code>
     *     settings ::= levelDescription (',' levelDescription)*
     *     levelDescription ::= level ('[' tagPrefixes ']')?
     *     level ::= 'none' | 'always' | 'error' | ...
     *     tagPrefix ::= [a-zA-Z_] [a-zA-Z_0-9:<>]*
     *     tagPrefixes ::= tagPrefix (',' tagPrefix)*
     * </code></pre>
     *
     * Notes:
     * - All whitespace is ignored, might be useful for formatting.
     * - Level without tags is considered as primary level.
     *
     * Example:
     * <pre><code>
     *     none, verbose[nx::media::media_player_quality_chooser], debug[ec2::DbManager]
     * </code></pre>
     */
    bool parse(const QString& s);

    template<typename ... Args>
    bool parse(const QString& arg, Args ... args)
    {
        if (arg.isEmpty())
            return parse(std::forward<Args>(args) ...);
        else
            return parse(arg);
    }
};

} // namespace log
} // namespace utils
} // namespace nx
