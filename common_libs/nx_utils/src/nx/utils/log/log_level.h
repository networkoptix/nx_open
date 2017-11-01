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

    template<typename T>
    Tag(const T* pointer): m_value(::toString(pointer)) {}

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
     *     levelDescription ::= level ('[' tags ']')?
     *     level ::= 'none' | 'always' | 'error' | ...
     *     tags ::= [a-zA-Z_:]+
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
    void parse(const QString& s);

    template<typename ... Args>
    void parse(const QString& arg, Args ... args)
    {
        if (arg.isEmpty())
            parse(std::forward<Args>(args) ...);
        else
            parse(arg);
    }
};

} // namespace log
} // namespace utils
} // namespace nx
