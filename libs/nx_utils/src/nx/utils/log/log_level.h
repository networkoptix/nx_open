#pragma once

#include <map>
#include <set>

#include <QtCore/QRegularExpression>

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

    Tag(const std::type_info& info);

    /** NOTE: Use a tag made from a string only when absolutely necessary. */
    explicit Tag(QString s);

    /** NOTE: Use a tag made from a string only when absolutely necessary. */
    explicit Tag(const std::string& s);

    /**
     * Disabled to prohibit compiling NX_DEBUG("message", ...) - incorrect because the message is
     * passed instead of the tag. Using `explicit` does not help because Tag(const T*) will be
     * called then, and the wrong call would compile.
     */
    Tag(const char* s) = delete;

    template<typename T>
    Tag(const T* pointer): m_value(::toString(pointer)) {}

    template<typename T>
    Tag join(const T* pointer) const { return Tag(m_value + "::" + ::toString(pointer)); }

    const QString& toString() const;
    Tag operator+(const Tag& rhs) const;

    bool operator<(const Tag& rhs) const;
    bool operator==(const Tag& rhs) const;
    bool operator!=(const Tag& rhs) const;

private:
    QString m_value;
};

class NX_UTILS_API Filter
{
public:
    explicit Filter(const QString& source, bool regexp = true);
    explicit Filter(const Tag& tag);
    explicit Filter(const QRegularExpression& source);

    bool isValid() const;
    bool accepts(const Tag& tag) const;
    QString toString() const;

    bool operator<(const Filter& rhs) const;
    bool operator==(const Filter& rhs) const;
    bool operator!=(const Filter& rhs) const;
private:
    enum class Mode
    {
        exactMatch,
        regexp
    };
    const Mode m_mode;
    const QString m_value;
    const QRegularExpression m_filter;
};
using LevelFilters = std::map<Filter, Level>;

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
