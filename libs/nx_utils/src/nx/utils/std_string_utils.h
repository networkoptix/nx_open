#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "buffer.h"
#include "std/charconv.h"

#if defined(ANDROID) || defined(__ANDROID__)
namespace std {
    template<typename... U> using is_invocable = __invokable_r<U...>;
    template<typename... U> inline constexpr bool is_invocable_v = __invokable_r<U...>::value;
}
#endif

namespace nx::utils {

template<template<typename...> class String, typename CharType>
// requires Random_access_iterator<typename String<Char>::iterator>
String<CharType> maxPrefix(const String<CharType>& one, const String<CharType>& two)
{
    auto oneIter = one.begin();
    auto twoIter = two.begin();

    while ((oneIter != one.end()) && (twoIter != two.end()) && (*oneIter == *twoIter))
    {
        ++oneIter;
        ++twoIter;
    }

    return String<CharType>(one.begin(), oneIter);
}

//-------------------------------------------------------------------------------------------------

template<typename CharType, typename IsSpaceFunc>
// requires std::is_invocable_v<IsSpaceFunc, char>
void trim(
    std::basic_string_view<CharType>* str,
    IsSpaceFunc f,
    typename std::enable_if<std::is_invocable_v<IsSpaceFunc, CharType>>::type* = nullptr)
{
    while (!str->empty() && f(str->front()))
        str->remove_prefix(1);
    while (!str->empty() && f(str->back()))
        str->remove_suffix(1);
}

template<typename CharType, typename IsSpaceFunc>
// requires std::is_invocable_v<IsSpaceFunc, char>
void trim(
    std::basic_string<CharType>* str,
    IsSpaceFunc f,
    typename std::enable_if<std::is_invocable_v<IsSpaceFunc, CharType>>::type* = nullptr)
{
    auto strView = (std::basic_string_view<CharType>) (*str);
    trim(&strView, std::move(f));
    *str = std::basic_string<CharType>(strView);
}

/**
 * Removes from start and end of the string all characters for which isspace(...) returns non-zero.
 */
template<template<typename...> class String, typename CharType>
void trim(String<CharType>* str)
{
    trim(str, [](CharType ch) { return isspace(ch) > 0; });
}

/**
 * Removes from start and end of the string all characters for which isspace(...) returns non-zero.
 */
template<template<typename...> class String, typename CharType, typename CharArray>
// requires std::is_array<CharArray>::value
void trim(
    String<CharType>* str,
    const CharArray& charArray,
    typename std::enable_if<std::is_array<CharArray>::value>::type* = nullptr)
{
    trim(str, [&charArray](CharType ch) {
        return std::find(std::begin(charArray), std::end(charArray), ch) != std::end(charArray);
    });
}

template<typename CharType, typename... Args>
std::basic_string_view<CharType> trim(
    const std::basic_string_view<CharType>& str,
    Args&&... args)
{
    auto result = str;
    trim(&result, std::forward<Args>(args)...);
    return result;
}

template<typename CharType, typename... Args>
std::basic_string<CharType> trim(
    const std::basic_string<CharType>& str,
    Args&& ... args)
{
    auto view = (std::basic_string_view<CharType>) str;
    trim(&view, std::forward<Args>(args)...);
    return std::basic_string<CharType>(view);
}

/**
 * WARNING: The result is a reference to str. So, the str MUST be kept alive by the caller.
 * If it is not the case, then consider using trim(const std::string&).
 */
template<typename CharType, std::size_t N, typename... Args>
std::string_view trim(
    CharType (&str)[N],
    Args&& ... args)
{
    return trim(std::string_view(str), std::forward<Args>(args)...);
}

//-------------------------------------------------------------------------------------------------

namespace separator {

template<typename Str>
class IsAnyOf
{
public:
    IsAnyOf(const Str& str): m_str(str) {}

    bool operator()(char ch) const { return std::strchr(m_str, ch) != nullptr; }

private:
    const Str& m_str;
};

template<>
class IsAnyOf<char>
{
public:
    IsAnyOf(char ch): m_ch(ch) {}

    bool operator()(char ch) const { return ch == m_ch; }

private:
    char m_ch;
};

template <typename Value>
IsAnyOf<Value> isAnyOf(const Value& value)
{
    return IsAnyOf<Value>(value);
}

} // namespace separator

enum GroupToken
{
    none = 0,
    doubleQuotes = 1,
    singleQuotes = 2,
    squareBrackets = 4,
    roundBrackets = 8,
};

enum SplitterFlag
{
    noFlags = 0,
    skipEmpty = 1,
};

/**
 * Splits text by a separator.
 * Usage example:
 * <pre><code>
 *   auto splitter = makeSplitter(str, separator);
 *   while (splitter.next())
 *       handle(splitter.token());
 * </code></pre>
 *
 * @param Separator char or any type from the separator namespace.
 *
 * NOTE: Skips empty tokens.
 * NOTE: Does no allocations, operates only views to the source string.
 */
template<
    template<typename...> class String, typename CharType,
    typename Separator
>
class Splitter
{
public:
    Splitter(
        const String<CharType>& str,
        Separator sep,
        int groupTokens = GroupToken::none,
        int flags = SplitterFlag::skipEmpty)
        :
        m_str(str),
        m_sep(std::move(sep)),
        m_groupTokens(groupTokens),
        m_flags(flags)
    {
    }

    bool next()
    {
        char currentGroupClosingToken = 0;
        for (; m_pos < m_str.size(); ++m_pos)
        {
            if (m_tokenStart == String<CharType>::npos)
                m_tokenStart = m_pos;

            const auto ch = m_str[m_pos];

            if (currentGroupClosingToken)
            {
                if (ch == currentGroupClosingToken)
                    currentGroupClosingToken = 0;
                continue;
            }

            // TODO: #ak Add support for the multi-character separator.
            // It means the rest of the string should be passed here.
            // The complexity will be len(text)*len(separator).
            if (m_sep(ch))
            {
                if (m_pos > m_tokenStart || ((m_flags & SplitterFlag::skipEmpty) == 0))
                    break; //< A token found.

                // Ignoring an empty token.
                m_tokenStart = String<CharType>::npos;
            }

            // TODO: #ak Optimize this block. E.g., this can be a table lookup.
            if (((m_groupTokens & GroupToken::doubleQuotes) && ch == '"') ||
                ((m_groupTokens & GroupToken::singleQuotes) && ch == '\'') ||
                ((m_groupTokens & GroupToken::squareBrackets) && ch == '[') ||
                ((m_groupTokens & GroupToken::roundBrackets) && ch == '('))
            {
                currentGroupClosingToken =
                    ch == '"' ? '"' :
                    ch == '\'' ? '\'' :
                    ch == '[' ? ']' :
                    ')'; // ch == '('
            }
        }

        if (m_tokenStart != String<CharType>::npos &&
            (m_pos > m_tokenStart || ((m_flags & SplitterFlag::skipEmpty) == 0)))
        {
            m_token = std::basic_string_view<CharType>(
                m_str.data() + m_tokenStart, m_pos - m_tokenStart);
            m_tokenStart = String<CharType>::npos;
            ++m_pos;
            if (m_pos == m_str.size()) //< A special case for a separator at the end.
                m_tokenStart = m_str.size();
            return true;
        }

        return false;
    }

    const std::basic_string_view<CharType>& token() const
    {
        return m_token;
    }

private:
    const String<CharType>& m_str;
    Separator m_sep;
    typename String<CharType>::size_type m_tokenStart = String<CharType>::npos;
    typename String<CharType>::size_type m_pos = 0;
    std::basic_string_view<CharType> m_token;
    const int m_groupTokens = 0;
    const int m_flags = 0;
};

template<
    template<typename...> class String, typename CharType
>
class Splitter<String, CharType, char>:
    public Splitter<String, CharType, separator::IsAnyOf<char>>
{
    using base_type = Splitter<String, CharType, separator::IsAnyOf<char>>;

public:
    Splitter(
        const String<CharType>& str,
        char ch,
        int groupTokens = GroupToken::none,
        int flags = SplitterFlag::skipEmpty)
        :
        base_type(str, separator::IsAnyOf<char>(ch), groupTokens, flags)
    {
    }
};

template<
    template<typename...> class String, typename CharType,
    typename Separator
>
auto makeSplitter(
    const String<CharType>& str,
    Separator sep,
    int groupToken = GroupToken::none,
    int flags = SplitterFlag::skipEmpty)
{
    return Splitter<String, CharType, Separator>(str, std::move(sep), groupToken, flags);
}

//-------------------------------------------------------------------------------------------------

namespace detail {

/**
 * Introducing this function as a work around notorious msvc bug with "else if constexpr"
 * inside lambda inside a template function.
 */
template <typename InsertIterator, typename CharType>
void saveToken(
    InsertIterator& outIter,
    const std::basic_string_view<CharType>& token)
{
    if constexpr (std::is_assignable<decltype(*outIter), std::basic_string_view<CharType>>::value)
        *outIter++ = token;
    else if constexpr (std::is_assignable<decltype(*outIter), std::basic_string<CharType>>::value)
        *outIter++ = std::basic_string<CharType>(token);
    else
        *outIter++ = token;
}

} // namespace detail

/**
 * Invokes f(const std::string_view&) for each entry of str as separated by the separator.
 * Uses nx::utils::Splitter class inside.
 */
template<
    template<typename...> class String, typename CharType,
    typename Separator,
    typename UnaryFunction
>
// requires std::is_invocable_v<UnaryFunction, std::basic_string_view<CharType>>
void split(
    const String<CharType>& str,
    Separator sep,
    UnaryFunction f,
    int groupToken = GroupToken::none,
    int flags = SplitterFlag::skipEmpty,
    decltype(f(std::basic_string_view<CharType>()))* = nullptr)
{
    auto splitter = makeSplitter(str, sep, groupToken, flags);
    while (splitter.next())
        f(splitter.token());
}

template<
    template<typename...> class String, typename CharType,
    typename Separator,
    typename OutputIt
>
// requires Iterator<OutputIt>
void split(
    const String<CharType>& str,
    Separator sep,
    OutputIt outIter,
    int groupToken = GroupToken::none,
    int flags = SplitterFlag::skipEmpty,
    typename std::iterator_traits<OutputIt>::value_type* = nullptr)
{
    split(
        str, sep,
        [&outIter](const std::basic_string_view<CharType>& token) mutable
        {
            detail::saveToken(outIter, token);
        },
        groupToken,
        flags);
}

/**
 * Splits the input string, providing limited number of tokens (maxTokenCount).
 * @return std::tuple<token array, token in the array count>.
 * NOTE: With this function there is no way to known if all tokens have been read from
 * the source string or not because the parsing stops on maxTokenCount.
 * WARNING: The resulting token is always represented with std::basic_string_view!
 *   So, it references the source string.
 */
template<
    std::size_t maxTokenCount,
    template<typename...> class String, typename CharType,
    typename Separator
>
[[nodiscard]] std::tuple<
    std::array<std::basic_string_view<CharType>, maxTokenCount>,
    std::size_t
>
split_n(
    const String<CharType>& str,
    Separator sep,
    int groupToken = GroupToken::none,
    int flags = SplitterFlag::skipEmpty)
{
    std::array<std::basic_string_view<CharType>, maxTokenCount> tokens;
    std::size_t pos = 0;

    auto splitter = makeSplitter(str, sep, groupToken, flags);
    for (; splitter.next() && pos < maxTokenCount; ++pos)
        tokens[pos] = (splitter.token());

    return std::make_tuple(std::move(tokens), pos);
}

// NOTE: Forbidding invoking split_n with rvalue since returning references to the input.
template<
    std::size_t maxTokenCount,
    template<typename...> class String, typename CharType,
    typename Separator
>
[[nodiscard]] std::tuple<
    std::array<std::basic_string_view<CharType>, maxTokenCount>,
    std::size_t
>
split_n(
    String<CharType>&& str,
    Separator sep,
    int groupToken = GroupToken::none,
    int flags = SplitterFlag::skipEmpty) = delete;

//-------------------------------------------------------------------------------------------------

namespace detail {

/**
 * Introducing this function as a work around notorious msvc bug with "else if constexpr"
 * inside lambda inside a template function.
 */
template <typename InsertIterator, typename CharType>
void saveNameValuePair(
    InsertIterator& outIter,
    const std::basic_string_view<CharType>& name,
    const std::basic_string_view<CharType>& value)
{
    using ViewPair = std::pair<std::basic_string_view<CharType>, std::basic_string_view<CharType>>;
    using StringPair = std::pair<std::basic_string<CharType>, std::basic_string<CharType>>;

    if constexpr (std::is_assignable<decltype(*outIter), ViewPair>::value)
        *outIter++ = {name, value};
    else if constexpr (std::is_assignable<decltype(*outIter), StringPair>::value)
        *outIter++ = {std::basic_string<CharType>(name), std::basic_string<CharType>(value)};
    else
        *outIter++ = {name, value};
}

} // namespace detail

/**
 * Splits a list of name/value pairs and reports them to f.
 *
 * Example:
 * <pre><code>
 * std::vector<std::string, std::string> params;
 * splitNameValuePairs(
 *     std::string("a=b,c=\"d,\""), ',', '=',
 *     [&params](const std::string_view& name, const std::string_view& value)
 *     {
 *         params.emplace(name, value);
 *     });
 *
 * assert(params == {{"a", "b"}, {"c", "d,"}});
 * </code></pre>
 *
 * Note that the value if trimmed of group tokens (double quotes, by default).
 */
template<
    template<typename...> class String, typename CharType,
    typename PairSeparator,
    typename NameValueSeparator,
    typename Function
>
// requires std::is_invocable_v<Function, std::basic_string_view<CharType>, std::basic_string_view<CharType>>
void splitNameValuePairs(
    const String<CharType>& str,
    PairSeparator pairSeparator,
    NameValueSeparator nameValueSeparator,
    Function f,
    int groupTokens = GroupToken::doubleQuotes,
    typename std::enable_if<std::is_invocable_v<Function,
        std::basic_string_view<CharType>, std::basic_string_view<CharType>>>::type* = nullptr)
{
    split(
        str, pairSeparator,
        [nameValueSeparator = std::move(nameValueSeparator), f = std::move(f), groupTokens](
            const std::basic_string_view<CharType>& nameValue) mutable
        {
            const auto [tokens, count] = split_n<2>(nameValue, nameValueSeparator, groupTokens);

            std::basic_string_view<CharType> value;
            if (count > 1)
            {
                // Trimming the value of group tokens.
                // TODO: #ak Optimize trimming. Probably, it is possible to compose compile-time
                // array of chars to trim.

                value = trim(
                    trim(tokens[1]), //< Trimming of spaces.
                    [groupTokens](CharType ch)
                    {
                        return ((groupTokens & GroupToken::doubleQuotes) && ch == '"')
                            || ((groupTokens & GroupToken::singleQuotes) && ch == '\'')
                            || ((groupTokens & GroupToken::roundBrackets) && (ch == '(' || ch == ')'))
                            || ((groupTokens & GroupToken::squareBrackets) && (ch == '[' || ch == ']'));
                    });
            }

            f(trim(tokens[0]), value);
        },
        groupTokens);
}

/**
 * splitNameValuePairs overload that saves pair into an output iterator.
 */
template<
    template<typename...> class String, typename CharType,
    typename PairSeparator,
    typename NameValueSeparator,
    typename OutputIt
>
// requires Iterator<OutputIt>
void splitNameValuePairs(
    const String<CharType>& str,
    PairSeparator pairSeparator,
    NameValueSeparator nameValueSeparator,
    OutputIt outIter,
    int groupTokens = GroupToken::doubleQuotes,
    typename std::enable_if<!std::is_invocable_v<OutputIt,
        std::basic_string_view<CharType>, std::basic_string_view<CharType>>>::type* = nullptr)
{
    splitNameValuePairs(
        str, std::move(pairSeparator), std::move(nameValueSeparator),
        [outIter = std::move(outIter)](
            const std::basic_string_view<CharType>& name,
            const std::basic_string_view<CharType>& value) mutable
        {
            detail::saveNameValuePair(outIter, name, value);
        },
        groupTokens);
}

//-------------------------------------------------------------------------------------------------

namespace detail {

inline std::size_t calculateLength(const std::string& str)
{
    return str.size();
}

inline std::size_t calculateLength(const std::string_view& str)
{
    return str.size();
}

inline std::size_t calculateLength(const char* str)
{
    return std::strlen(str);
}

inline std::size_t calculateLength(char ch)
{
    return sizeof(ch);
}

template<typename T>
// requires std::is_integral_v<T>
std::size_t calculateLength(
    const T& /*val*/,
    std::enable_if_t<std::is_integral_v<T>>* = nullptr)
{
    return sizeof(T) * 3;
}

template<typename T>
std::size_t calculateLength(const std::optional<T>& val)
{
    return val ? calculateLength(*val) : 0;
}

template<typename T,
    typename X = std::enable_if_t<std::is_member_function_pointer_v<decltype(&T::size)>>
>
std::size_t calculateLength(const T& val)
{
    return val.size();
}

//-------------------------------------------------------------------------------------------------

template<typename String>
void append(String* str, const std::string& val)
{
    *str += val;
}

template<typename String>
void append(String* str, const std::string_view& val)
{
    *str += val;
}

template<typename String, typename T,
    typename X = std::enable_if_t<std::is_same_v<T, char>>
>
void append(String* str, const T* val)
{
    *str += val;
}

template<typename String>
void append(String* str, const char val)
{
    *str += val;
}

template<typename String, typename T>
// requires std::is_integral_v<T>
void append(
    String* str, T val,
    std::enable_if_t<std::is_integral_v<T>>* = nullptr)
{
    char buf[sizeof(val) * 3];
    const auto result = charconv::to_chars(buf, buf + sizeof(buf), val);
    if (result.ec == std::errc())
        str->append(buf, result.ptr - buf);
}

template<typename String, typename T>
void append(String* str, const std::optional<T>& val)
{
    if (val)
        append(str, *val);
}

template<typename String, typename T,
    //typename X = std::enable_if_t<std::is_member_function_pointer_v<decltype(&T::data)>>,
    typename Y = std::enable_if_t<std::is_member_function_pointer_v<decltype(&T::size)>>
>
void append(String* str, const T& val)
{
    append(str, std::string_view(val.data(), val.size()));
}

//-------------------------------------------------------------------------------------------------

template<typename T>
constexpr bool is_initialized(const T& /*value*/)
{
    return true;
}

template<typename T>
constexpr bool is_initialized(const std::optional<T>& value)
{
    return (bool) value;
}

} // namespace detail

/**
 * Builds std::string from any combination of std::string, std::string_view, const char*, char,
 * performing only a single allocation.
 * The resulting string is appended to out.
 * Note that when using const char* this function has to calculate the length of const char* first.
 */
template<typename String, typename... Tokens>
void buildStringInPlace(
    String* out,
    const Tokens& ... tokens)
{
    // Calculating resulting string length.
    std::initializer_list<std::size_t> tokenLengths{ detail::calculateLength(tokens) ... };
    const auto length = std::accumulate(tokenLengths.begin(), tokenLengths.end(), (std::size_t) 0);

    if (out->capacity() - out->size() < length + 1)
        out->reserve(out->size() + length + 1);

    auto f = [out](const auto& token) { detail::append(out, token); };
    (void) std::initializer_list<int>{(f(tokens), 1) ...};
}

template<typename... Tokens>
void buildString(std::string* out, const Tokens&... tokens)
{
    buildStringInPlace(out, tokens...);
}

template<typename... Tokens>
void buildString(BasicBuffer<char>* out, const Tokens& ... tokens)
{
    buildStringInPlace(out, tokens...);
}

template<typename String = std::string, typename... Tokens>
[[nodiscard]] String buildString(const Tokens&... tokens)
{
    // Building the string.
    String result;
    buildString(&result, tokens...);
    return result;
}

//-------------------------------------------------------------------------------------------------

namespace detail {

template <typename Value, typename FieldType>
// requires !std::is_invocable_v<FieldType, Value>
std::size_t calculateLength(
    const Value&, const FieldType& field,
    std::enable_if_t<!std::is_invocable_v<FieldType, Value>>* = nullptr)
{
    return calculateLength(field);
}

template<typename Value, typename Accessor>
// requires std::is_invocable_v<Accessor, Value>
std::size_t calculateLength(
    const Value& value,
    const Accessor& accessor,
    std::enable_if_t<std::is_invocable_v<Accessor, Value>>* = nullptr)
{
    return calculateLength(accessor(value));
}

template<typename Value, typename Accessor, typename String>
// requires std::is_invocable_v<Accessor, Value>
void append(
    const Value& value,
    const Accessor& accessor,
    String* result,
    std::enable_if_t<std::is_invocable_v<Accessor, Value>>* = nullptr)
{
    append(result, accessor(value));
}

template<typename Value, typename Field, typename String>
// requires !std::is_invocable_v<Field, Value>
void append(
    const Value& /*value*/,
    const Field& field,
    String* result,
    std::enable_if_t<!std::is_invocable_v<Field, Value>>* = nullptr)
{
    append(result, field);
}

} // namespace detail

/**
 * Joins sequence of records, also joining fields of each record as specified by recordStringizers.
 * @param out The resulting string is appended to out.
 * @param recordSeparator string or string constant.
 * @param recordStringizers Specifies joining fields of a record.
 *   This can include strings, string constants and record member accessors (std::mem_fn).
 *   This argument may be ommitted if joining a sequence of strings.
 * Member accessor MUST provide some string type.
 *
 * Example:
 * <pre><code>
 * struct Record { std::string a; std::string b; }
 * std::vector<Record> records = {{"a1", "b1"}, {{"a2", "b2"}}};
 *
 * std::string str;
 * nx::utils::join(
 *   &str,
 *   records.begin(), records.end(), ";",
 *   "a=", std::mem_fn(&Record::a), ',', "b=", std::mem_fn(&Record::b));
 *
 * assert(str == "a=a1,b=b1;a=a2,b=b2");
 * </code></pre>
 *
 * NOTE: Precalculates resulting string size first to minimize the number of allocations.
 *   To achieve this, the range [begin, end) is scanned twice.
 *   First time - to calculate the resulting string length, the second time - to concatenate.
 */
template<
    typename String,
    typename InputIter,
    typename RecordSeparator,
    typename... RecordStringizers
>
void joinInPlace(
    String* out,
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator,
    const RecordStringizers&... recordStringizers)
{
    // 1. calculating length.
    std::size_t length = 0;
    for (auto it = begin; it != end; ++it)
    {
        const auto& value = *it;

        if (!detail::is_initialized(value))
            continue;

        if (it != begin)
            length += detail::calculateLength(recordSeparator);

        if constexpr (sizeof...(RecordStringizers) > 0)
        {
            std::initializer_list<std::size_t> tokenLengths{
                detail::calculateLength(value, recordStringizers)... };
            length = std::accumulate(tokenLengths.begin(), tokenLengths.end(), length);
        }
        else
        {
            length += detail::calculateLength(value);
        }
    }

    if (out->capacity() - out->size() < length + 1)
        out->reserve(out->size() + length + 1);

    // 2. building the string.
    for (auto it = begin; it != end; ++it)
    {
        const auto& value = *it;

        if (!detail::is_initialized(value))
            continue;

        if (it != begin)
            detail::append(out, recordSeparator);

        if constexpr (sizeof...(RecordStringizers) > 0)
        {
            (void) std::initializer_list<int>{
                (detail::append(value, recordStringizers, out), 1) ...};
        }
        else
        {
            detail::append(out, value);
        }
    }
}

template<
    typename InputIter,
    typename RecordSeparator,
    typename... RecordStringizers
>
void join(
    std::string* out,
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator,
    const RecordStringizers&... recordStringizers)
{
    joinInPlace(out, begin, end, recordSeparator, recordStringizers...);
}

template<
    typename InputIter,
    typename RecordSeparator,
    typename... RecordStringizers
>
void join(
    BasicBuffer<char>* out,
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator,
    const RecordStringizers&... recordStringizers)
{
    joinInPlace(out, begin, end, recordSeparator, recordStringizers...);
}

template<
    typename InputIter,
    typename RecordSeparator,
    typename... RecordStringizers
>
std::string join(
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator,
    const RecordStringizers&... recordStringizers)
{
    std::string result;
    joinInPlace(
        &result,
        begin, end,
        recordSeparator,
        recordStringizers...);
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * E.g., reverseWords("test.example.com", '.') returns "com.example.test".
 */
template<
    template<typename...> class String, typename CharType,
    typename Separator>
std::basic_string<CharType> reverseWords(
    const String<CharType>& str,
    Separator separator)
{
    std::vector<std::basic_string_view<CharType>> tokens;
    split(str, separator, std::back_inserter(tokens), GroupToken::none, SplitterFlag::noFlags);
    return join(tokens.rbegin(), tokens.rend(), separator);
}

// TODO: #ak Provide inplace version of reverseWords since output has the same size as input.

//-------------------------------------------------------------------------------------------------

template<typename StringType, typename Suffix>
// requires String<StringType>
bool endsWith(const StringType& str, const Suffix& suffix) = delete;

template<typename StringType>
// requires String<StringType>
bool endsWith(const StringType& str, const std::string_view& suffix)
{
    const auto size = str.size();
    if (size < suffix.size())
        return false;

    return std::memcmp(
        str.data() + size - suffix.size(),
        suffix.data(),
        suffix.size()) == 0;
}

template<typename StringType>
// requires String<StringType>
bool endsWith(const StringType& str, char suffix)
{
    if (str.empty())
        return false;

    return str.back() == suffix;
}

template<typename StringType>
// requires String<StringType>
bool endsWith(const StringType& str, const std::string& suffix)
{
    return endsWith(str, (std::string_view) suffix);
}

template<typename StringType>
// requires String<StringType>
bool endsWith(const StringType& str, const char* suffix)
{
    return endsWith(str, std::string_view(suffix));
}

//-------------------------------------------------------------------------------------------------

template<typename StringType, typename Prefix>
// requires String<StringType>
bool startsWith(const StringType& str, const Prefix& prefix) = delete;

template<typename StringType>
// requires String<StringType>
bool startsWith(const StringType& str, const std::string_view& prefix)
{
    const auto size = str.size();
    if (size < prefix.size())
        return false;

    return std::memcmp(
        str.data(),
        prefix.data(),
        prefix.size()) == 0;
}

template<typename StringType>
// requires String<StringType>
bool startsWith(const StringType& str, char prefix)
{
    if (str.empty())
        return false;

    return str.front() == prefix;
}

template<typename StringType>
// requires String<StringType>
bool startsWith(const StringType& str, const std::string& prefix)
{
    return startsWith(str, (std::string_view) prefix);
}

template<typename StringType>
// requires String<StringType>
bool startsWith(const StringType& str, const char* prefix)
{
    return startsWith(str, std::string_view(prefix));
}

//-------------------------------------------------------------------------------------------------

template<typename Value, typename Arg>
// requires Numeric<Value>
Value ston(const std::string_view& str, std::size_t* pos, Arg arg)
{
    auto value = Value();
    const auto result = charconv::from_chars(str.data(), str.data() + str.size(), value, arg);
    if (pos)
        *pos = result.ptr - str.data();
    return value;
}

/**
 * Parses str as an integer, skipping spaces in the front of the str.
 * @param pos If not nullptr, set to the position of the first non-digit character
 *   (or str.size() if the whole str has been parsed).
 *   Set to 0 in case of a parse error.
 *
 * This function differs from std::stoi in the following:
 * - Accepts std::string_view
 * - Does not throw. The error is reported by setting *pos to 0.
 */
inline int stoi(const std::string_view& str, std::size_t* pos = nullptr, int base = 10)
{
    return ston<int>(str, pos, base);
}

/** See nx::utils::stoi. */
inline long stol(const std::string_view& str, std::size_t* pos = nullptr, int base = 10)
{
    return ston<long>(str, pos, base);
}

/** See nx::utils::stoi. */
inline long long stoll(const std::string_view& str, std::size_t* pos = nullptr, int base = 10)
{
    return ston<long long>(str, pos, base);
}

/** See nx::utils::stoi. */
inline unsigned long stoul(const std::string_view& str, std::size_t* pos = nullptr, int base = 10)
{
    return ston<unsigned long>(str, pos, base);
}

/** See nx::utils::stoi. */
inline unsigned long long stoull(const std::string_view& str, std::size_t* pos = nullptr, int base = 10)
{
    return ston<unsigned long long>(str, pos, base);
}

/** See nx::utils::stoi. */
inline double stod(
    const std::string_view& str,
    std::size_t* pos = nullptr)
{
    // NOTE: GCC 8.1 does not provide the corresponding charconv::from_chars.

    static constexpr std::size_t kMaxStrLength = 32;

    // Avoiding using heap.
    char buf[kMaxStrLength];
    const auto bytesToCopy = std::min(str.size(), kMaxStrLength-1);
    memcpy(buf, str.data(), bytesToCopy);
    buf[bytesToCopy] = 0;

    char* end = nullptr;
    const auto val = std::strtod(buf, &end);
    if (pos)
        *pos = end - buf;
    return val;
}

//-------------------------------------------------------------------------------------------------

template<typename T, typename X = std::enable_if_t<std::is_integral_v<T>>>
std::string to_string(T value, int base = 10)
{
    std::string str(sizeof(value) * 3, '\0');
    const auto result = charconv::to_chars(str.data(), str.data() + str.size(), value, base);
    str.resize(result.ptr - str.data());
    return str;
}

template<typename T, typename X = std::enable_if_t<std::is_floating_point_v<T>>>
std::string to_string(T value)
{
    return std::to_string(value);
}

//-------------------------------------------------------------------------------------------------

/**
 * Compares two strings case-insensitive.
 * @return 0 if strings are equal, negative value if left < right, positive if left > right.
 */
NX_UTILS_API int stricmp(const std::string_view& left, const std::string_view& right);

//-------------------------------------------------------------------------------------------------

//template<template<typename...> class String>
//std::string toHex(const String<char>& str)
inline std::string toHex(const std::string_view& str)
{
    static constexpr char kDigits[] = "0123456789abcdef";

    std::string result;
    result.resize(str.size() * 2);

    std::size_t pos = 0;
    for (auto ch: str)
    {
        result[pos++] = kDigits[(ch >> 4) & 0x0f];
        result[pos++] = kDigits[ch & 0x0f];
    }

    return result;
}

inline BasicBuffer<char> toHex(const BasicBuffer<char>& str)
{
    return BasicBuffer<char>(toHex(std::string_view(str.data(), str.size())));
}

inline std::string fromHex(const std::string_view& str)
{
    // TODO: #ak Get rid of QByteArray. It will save 2 allocations in this function.
    return QByteArray::fromHex(QByteArray::fromRawData(str.data(), (int) str.size()))
        .toStdString();
}

template<typename Value>
// requires std::is_unsigned<Value>::value
std::string toHex(
    Value value,
    typename std::enable_if<std::is_unsigned<Value>::value>::type* = nullptr)
{
    static constexpr char kDigits[] = "0123456789abcdef";
    static constexpr std::size_t kMaxHexLen = sizeof(value) * 2;

    if (value == 0)
        return std::string(1, '0');

    char str[kMaxHexLen];

    std::size_t pos = kMaxHexLen - 1;
    for (; value > 0; --pos, value >>= 4)
        str[pos] = kDigits[value & 0x0f];

    return std::string(str + pos, sizeof(str) - pos);
}

//-------------------------------------------------------------------------------------------------

// TODO: #ak Get rid of QByteArray in base64 functions. It will save 2 allocations.
// TODO: #ak Introduce inplace conversion. That will allow implementing fromBase64(String&&)
// without allocations at all.
// TODO: #ak Provide single implementation.

inline std::string toBase64(const std::string_view& str)
{
    return QByteArray::fromRawData(str.data(), (int)str.size()).toBase64().toStdString();
}

inline std::string fromBase64(const std::string_view& str)
{
    return QByteArray::fromBase64(QByteArray::fromRawData(str.data(), (int)str.size()))
        .toStdString();
}

inline BasicBuffer<char> toBase64(const BasicBuffer<char>& str)
{
    return BasicBuffer<char>(QByteArray::fromRawData(str.data(), (int)str.size()).toBase64());
}

inline BasicBuffer<char> fromBase64(const BasicBuffer<char>& str)
{
    return BasicBuffer<char>(QByteArray::fromBase64(QByteArray::fromRawData(str.data(), (int)str.size())));
}

//-------------------------------------------------------------------------------------------------

/**
 * Converts ASCII string to low case. Does not support UTF-8.
 */
template<typename StringType>
// requires String<StringType>
void toLower(StringType* str)
{
    std::transform(str->begin(), str->end(), str->begin(), &tolower);
}

template<typename StringType>
// requires String<RealStringType>
auto toLower(StringType&& str)
{
    using RealStringType = std::remove_cv_t<std::remove_reference_t<StringType>>;

    auto result = RealStringType(std::forward<StringType>(str));
    toLower(&result);
    return result;
}

inline std::string toLower(const std::string_view& str)
{
    std::string result(str);
    toLower(&result);
    return result;
}

/**
 * Converts ASCII string to upper case. Does not support UTF-8.
 */
template<typename StringType>
// requires String<StringType>
void toUpper(StringType* str)
{
    std::transform(str->begin(), str->end(), str->begin(), &toupper);
}

template<typename StringType>
// requires String<RealStringType>
auto toUpper(StringType&& str)
{
    using RealStringType = std::remove_cv_t<std::remove_reference_t<StringType>>;

    auto result = RealStringType(std::forward<StringType>(str));
    toUpper(&result);
    return result;
}

inline std::string toUpper(const std::string_view& str)
{
    std::string result(str);
    toUpper(&result);
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * @return true if text contains str.
 */
template<typename CharType, typename String>
bool contains(const std::basic_string_view<CharType>& text, const String& str)
{
    return text.find(str) != text.npos;
}

/**
 * @return true if text contains str.
 */
template<typename CharType, typename String>
bool contains(const std::basic_string<CharType>& text, const String& str)
{
    return text.find(str) != text.npos;
}

//-------------------------------------------------------------------------------------------------

// TODO: #ak Provide implementation that accepts std::string_view.
inline std::string replace(
    const std::string& text,
    const std::string& token,
    const std::string& replacement)
{
    return std::regex_replace(text, std::regex(token), replacement);
}

} // namespace nx::utils

//-------------------------------------------------------------------------------------------------
// Convenience comparison operators for Qt types against STL types.

#if defined(QT_CORE_LIB)

inline bool operator==(const QByteArray& left, const std::string_view& right)
{ return left == QByteArray::fromRawData(right.data(), (int)right.size()); }

inline bool operator==(const std::string_view& left, const QByteArray& right)
{ return right == left; }

inline bool operator!=(const QByteArray& left, const std::string_view& right)
{ return !(left == right); }

inline bool operator!=(const std::string_view& left, const QByteArray& right)
{ return !(left == right); }

inline bool operator==(const QByteArray& left, const std::string& right)
{ return left == QByteArray::fromRawData(right.data(), (int)right.size()); }

inline bool operator==(const std::string& left, const QByteArray& right)
{ return right == left; }

inline bool operator!=(const QByteArray& left, const std::string& right)
{ return !(left == right); }

inline bool operator!=(const std::string& left, const QByteArray& right)
{ return !(left == right); }

//-------------------------------------------------------------------------------------------------
// QString vs std::string

inline bool operator==(const QString& left, const std::string& right)
{ return left == right.c_str(); }

inline bool operator==(const std::string& left, const QString& right)
{ return right == left; }

inline bool operator!=(const QString& left, const std::string& right)
{ return !(left == right); }

inline bool operator!=(const std::string& left, const QString& right)
{ return !(left == right); }

//-------------------------------------------------------------------------------------------------
// QString vs std::string_view

inline bool operator==(const QString& left, const std::string_view& right)
{ return left == QByteArray::fromRawData(right.data(), (int) right.size()); }

inline bool operator==(const std::string_view& left, const QString& right)
{ return right == left; }

inline bool operator!=(const QString& left, const std::string_view& right)
{ return !(left == right); }

inline bool operator!=(const std::string_view& left, const QString& right)
{ return !(left == right); }

//-------------------------------------------------------------------------------------------------
// std::string_view vs const char*. Needed since previous introducing "QString == std::string_view"
// makes "std::string_view == const char*" ambigious.

inline bool operator==(const std::string_view& left, const char* right)
{ return left == std::string_view(right); }

inline bool operator==(const char* left, const std::string_view& right)
{ return right == left; }

inline bool operator!=(const char* left, const std::string_view& right)
{ return !(left == right); }

inline bool operator!=(const std::string_view& left, const char* right)
{ return !(left == right); }

#endif // #if defined(QT_CORE_LIB)
