// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "buffer.h"
#include "std/charconv.h"
#include "type_traits.h"
#include "type_utils.h"

namespace nx::utils {

template<template<typename...> class String, typename CharType>
// requires Random_access_iterator<typename String<CharType>::iterator>
String<CharType> maxCommonPrefix(const String<CharType>& one, const String<CharType>& two)
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
    std::enable_if_t<std::is_invocable_v<IsSpaceFunc, CharType>>* = nullptr)
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
    std::enable_if_t<std::is_invocable_v<IsSpaceFunc, CharType>>* = nullptr)
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
    using UnsignedCharType = typename std::make_unsigned<CharType>::type;
    trim(str, [](CharType ch) { return isspace((UnsignedCharType) ch) > 0; });
}

/**
 * Removes from start and end of the string all characters for which isspace(...) returns non-zero.
 */
template<template<typename...> class String, typename CharType, typename CharArray>
// requires std::is_array_v<CharArray>
void trim(
    String<CharType>* str,
    const CharArray& charArray,
    std::enable_if_t<std::is_array_v<CharArray>>* = nullptr)
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

template<typename, typename = void>
class IsAnyOf;

template<typename Str>
class IsAnyOf<Str, std::void_t<decltype(std::declval<Str>().data())>>
{
public:
    IsAnyOf(const Str& str): m_str(str) {}

    bool operator()(char ch) const { return std::memchr(m_str.data(), ch, m_str.size()) != nullptr; }

private:
    const Str& m_str;
};

template<typename Str>
class IsAnyOf<Str, std::enable_if_t<std::is_same_v<Str, const char*> || std::is_array_v<Str>>>
{
public:
    IsAnyOf(const char* str): m_str(str) {}
    bool operator()(char ch) const { return std::strchr(m_str, ch) != nullptr; }
private:
    const char* m_str;
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

/**
 * Allows tokens which have separator characters when splitting.
 * See the comment to nx::utils::split function below for an example.
 */
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
 * NOTE: Skips empty tokens by default.
 * NOTE: Does no allocations, operates only views to the source string.
 */
template<
    template<typename...> class String, typename CharType,
    typename Separator
>
class Splitter
{
public:
    /**
     * See the comment to nx::utils::split function for arguments explanation.
     */
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

            // TODO: #akolesnikov Add support for the multi-character separator.
            // It means the rest of the string should be passed here.
            // The complexity will be len(text)*len(separator).
            if (m_sep(ch))
            {
                if (m_pos > m_tokenStart || ((m_flags & SplitterFlag::skipEmpty) == 0))
                    break; //< A token found.

                // Ignoring an empty token.
                m_tokenStart = String<CharType>::npos;
            }

            // TODO: #akolesnikov Optimize this block. E.g., this can be a table lookup.
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
    if constexpr (std::is_assignable_v<decltype(*outIter), std::basic_string_view<CharType>>)
        *outIter++ = token;
    else if constexpr (std::is_assignable_v<decltype(*outIter), std::basic_string<CharType>>)
        *outIter++ = std::basic_string<CharType>(token);
    else
        *outIter++ = token;
}

} // namespace detail

/**
 * Invokes f(const std::string_view&) for each entry of str as separated by the separator.
 * Uses nx::utils::Splitter class inside.
 * @param str The string to split.
 * @param groupToken Specifies symbols that are used to create tokens with a separator inside.
 *     See the usage example below.
 * @param flags A bitwise combination of values from SplitterFlag enumeration.
 *
 * Usage example:
 * <pre><code>
 * std::vector<std::string> tokens;
 * split(
 *     "a,,b,\"c,d\"",
 *     ',',
 *     [&](auto token) { tokens.push_back(token); },
 *     GroupToken::doubleQuotes);
 *
 * assert(tokens == {{"a", "b", "c,d"}});
 * </code></pre>
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
 * NOTE: The last token always contains the remainder of the string.
 * E.g., split_n<2>("a:b:c:d:", ":") returns ["a", "b:c:d:"].
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
    for (; pos < maxTokenCount && splitter.next(); ++pos)
        tokens[pos] = (splitter.token());

    if (pos > 0)
    {
        // adding the string remainder to the last token found.
        tokens[pos - 1] = std::basic_string_view<CharType>(
            tokens[pos - 1].data(),
            str.data() + str.size() - tokens[pos - 1].data());
    }

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

    if constexpr (std::is_assignable_v<decltype(*outIter), ViewPair>)
        *outIter++ = {name, value};
    else if constexpr (std::is_assignable_v<decltype(*outIter), StringPair>)
        *outIter++ = {std::basic_string<CharType>(name), std::basic_string<CharType>(value)};
    else
        *outIter++ = {name, value};
}

} // namespace detail

/**
 * Splits a list of name/value pairs and reports them to f.
 * @return Number of name/value pairs found.
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
 * Note that the value is trimmed of group tokens (double quotes, by default).
 */
template<
    template<typename...> class String, typename CharType,
    typename PairSeparator,
    typename NameValueSeparator,
    typename Function
>
// requires std::is_invocable_v<Function, std::basic_string_view<CharType>, std::basic_string_view<CharType>>
std::size_t splitNameValuePairs(
    const String<CharType>& str,
    PairSeparator pairSeparator,
    NameValueSeparator nameValueSeparator,
    Function f,
    int groupTokens = GroupToken::doubleQuotes,
    std::enable_if_t<std::is_invocable_v<Function,
        std::basic_string_view<CharType>, std::basic_string_view<CharType>>>* = nullptr)
{
    std::size_t pairCount = 0;

    split(
        str, pairSeparator,
        [nameValueSeparator = std::move(nameValueSeparator), &f, groupTokens, &pairCount](
            const std::basic_string_view<CharType>& nameValue) mutable
        {
            const auto [tokens, count] = split_n<2>(nameValue, nameValueSeparator, groupTokens);

            std::basic_string_view<CharType> value;
            if (count > 1)
            {
                // Trimming the value of group tokens.
                // TODO: #akolesnikov Optimize trimming. Probably, it is possible to compose compile-time
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

            ++pairCount;

            f(trim(tokens[0]), value);
        },
        groupTokens);

    return pairCount;
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
std::size_t splitNameValuePairs(
    const String<CharType>& str,
    PairSeparator pairSeparator,
    NameValueSeparator nameValueSeparator,
    OutputIt outIter,
    int groupTokens = GroupToken::doubleQuotes,
    std::enable_if_t<!std::is_invocable_v<OutputIt,
        std::basic_string_view<CharType>, std::basic_string_view<CharType>>>* = nullptr)
{
    return splitNameValuePairs(
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

template<typename String, typename T>
void append(
    String* str, const T* val,
    std::enable_if_t<std::is_same_v<T, char>>* = nullptr)
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
void append(String* str, const T& val,
    std::enable_if_t<
        IsConvertibleToStringView<T>::value ||
        HasToString<T>::value>* = nullptr)
{
    if constexpr (IsConvertibleToStringView<T>::value)
        append(str, std::string_view(val.data(), val.size()));
    else if constexpr (HasToString<T>::value)
        *str += val.toString();
}

template<typename String, typename T>
void append(String* str, const std::optional<T>& val)
{
    if (val)
        append(str, *val);
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

template<typename String, typename... Tokens>
void buildString(
    String* out,
    const Tokens& ... tokens)
{
    // Calculating resulting string length.
    std::initializer_list<std::size_t> tokenLengths{ detail::calculateLength(tokens) ... };
    const auto length = std::accumulate(tokenLengths.begin(), tokenLengths.end(), (std::size_t) 0);

    if (out->capacity() - out->size() < length + 1)
        out->reserve(out->size() + length + 1);

    auto f = [out](const auto& token) { detail::append(out, token); };
    (void)std::initializer_list<int>{(f(tokens), 1) ...};
}

} // namespace detail

/**
 * Builds std::string from any combination of std::string, std::string_view, const char*, char,
 * any integer type or a custom type that provides "const char* data() const" and "size() const"
 * (e.g., QByteArray).
 * Performs a single allocation only by precalculating the resulting string length.
 * The resulting string is appended to out.
 * Note that when using const char* this function has to calculate the length of const char* first.
 */
template<typename... Tokens>
void buildString(std::string* out, const Tokens&... tokens)
{
    detail::buildString(out, tokens...);
}

/**
 * Builds string and appends the result to BasicBuffer<char>.
 */
template<typename... Tokens>
void buildString(BasicBuffer<char>* out, const Tokens& ... tokens)
{
    detail::buildString(out, tokens...);
}

/**
 * Builds string and returns the result.
 */
template<typename String = std::string, typename... Tokens>
[[nodiscard]] String buildString(const Tokens&... tokens)
{
    String result;
    detail::buildString(&result, tokens...);
    return result;
}

template<typename... Args>
void append(Args&&... args)
{
    return buildString(std::forward<Args>(args)...);
}

//-------------------------------------------------------------------------------------------------

namespace detail {

template<
    std::size_t stringizerIndex,
    typename Field, typename OutString, typename Value, typename StringizersTuple
>
void stringizeRecursionStep(
    OutString* out,
    const Field& field,
    const Value& value,
    const StringizersTuple& recordStringizers,
    std::size_t estimatedOutStringLength);

template<typename String, typename Value, typename StringizersTuple,
    std::size_t stringizerIndex = std::tuple_size_v<StringizersTuple> - 1
>
void stringize(
    String* out,
    const Value& value,
    const StringizersTuple& recordStringizers,
    std::size_t estimatedOutStringLength)
{
    // Performing backward recursion on stringizers.

    using Stringizer = std::tuple_element_t<stringizerIndex, StringizersTuple>;

    if constexpr (std::is_invocable_v<Stringizer, Value>)
    {
        const auto& field = std::get<stringizerIndex>(recordStringizers)(value);
        stringizeRecursionStep<stringizerIndex>(
            out, field, value, recordStringizers, estimatedOutStringLength);
    }
    else
    {
        const auto& field = std::get<stringizerIndex>(recordStringizers);
        stringizeRecursionStep<stringizerIndex>(
            out, field, value, recordStringizers, estimatedOutStringLength);
    }
}

template<
    std::size_t stringizerIndex,
    typename Field, typename OutString, typename Value, typename StringizersTuple
>
void stringizeRecursionStep(
    OutString* out,
    const Field& field,
    const Value& value,
    const StringizersTuple& recordStringizers,
    std::size_t estimatedOutStringLength)
{
    const auto strLength = calculateLength(field);
    estimatedOutStringLength += strLength;

    if constexpr (stringizerIndex == 0)
    {
        out->reserve(out->size() + estimatedOutStringLength);
    }
    else
    {
        stringize<OutString, Value, StringizersTuple, stringizerIndex - 1>(
            out, value, recordStringizers, estimatedOutStringLength);
    }

    append(out, field);
}

template<typename String, typename Value, typename StringizersTuple>
void stringize(
    String* out,
    const Value& value,
    const StringizersTuple& recordStringizers)
{
    stringize(out, value, recordStringizers, 0);
}

template<
    typename String,
    typename InputIter,
    typename RecordSeparator,
    typename AppendFunc
>
void joinInternal(
    String* out,
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator,
    AppendFunc&& appendFunc)
{
    // Preallocating output string if possible.
    if constexpr (std::is_same_v<
        typename std::iterator_traits<InputIter>::iterator_category,
        std::random_access_iterator_tag>)
    {
        static constexpr std::size_t kPreallocatedCharsPerValue = 16;
        out->reserve(out->size() + std::distance(begin, end) * kPreallocatedCharsPerValue);
    }

    for (auto it = begin; it != end; ++it)
    {
        const auto& value = *it;

        if (!detail::is_initialized(value))
            continue;

        if (it != begin)
            append(out, recordSeparator);

        appendFunc(out, value);
    }
}

template<
    typename String,
    typename InputIter,
    typename RecordSeparator,
    typename... RecordStringizers
>
void join(
    String* out,
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator,
    RecordStringizers&&... recordStringizers)
{
    if constexpr (sizeof...(RecordStringizers) > 0)
    {
        const auto rs = std::make_tuple(std::forward<RecordStringizers>(recordStringizers)...);

        joinInternal(
            out, begin, end, recordSeparator,
            [&rs](String* out, const auto& value) { stringize(out, value, rs); });
    }
    else
    {
        joinInternal(
            out, begin, end, recordSeparator,
            [](String* out, const auto& value) { append(out, value); });
    }
}

template<
    typename String,
    typename InputIter,
    typename RecordSeparator,
    typename AppendFunc
>
void join(
    String* out,
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator,
    AppendFunc&& appendFunc,
    std::void_t<decltype(appendFunc(out, *begin))>* = nullptr)
{
    joinInternal(
        out, begin, end, recordSeparator,
        appendFunc);
}

} // namespace detail

/**
 * Joins sequence of values converting each value to a string by invoking a user lambda
 * or invoking "std::string Value::toString() const".
 * @param out The resulting string is appended to out.
 * @param recordSeparator string or string constant.
 * @param valueStringizer Lambda that converts value to std::string. See examples below.
 *
 * Usage examples:
 * <pre><code>
 * struct Record
 * {
 *     std::string a;
 *     std::string b;
 *
 *     std::string toString() const
 *     { return nx::utils::buildString("a=", a, ", b=", b); }
 * }
 * std::vector<Record> records = {{"a1", "b1"}, {{"a2", "b2"}}};
 *
 * // Example 1: the most efficient call (in number of allocations).
 * // If string representation of value is short (<= 16 characters per value on average),
 * // then a single allocation is performed.
 * std::string str1;
 * nx::utils::join(
 *   &str1,
 *   records.begin(), records.end(), ";",
 *   [](std::string* out, const auto& rec)
 *   {
 *       nx::utils::buildString(out, "a=", rec.a, ", b=", rec.b);
 *   });
 * assert(str1 == "a=a1,b=b1;a=a2,b=b2");
 *
 * // Example 2: one additional allocation per a value.
 * std::string str2;
 * nx::utils::join(
 *     records.begin(), records.end(), ";",
 *     [](const auto& rec) { return nx::utils::buildString("a=", rec.a, ", b=", rec.b); });
 * assert(str2 == "a=a1,b=b1;a=a2,b=b2");
 *
 * // Example 3: Record::toString() is invoked implicitly. One allocation per record.
 * std::string str3;
 * nx::utils::join(
 *   &str3,
 *   records.begin(), records.end(), ";");
 * assert(str3 == "a=a1,b=b1;a=a2,b=b2");
 *
 * // Example 4: the following convenience overload is available for each case:
 * std::string str4 = nx::utils::join(records.begin(), records.end(), ";");
 * assert(str4 == "a=a1,b=b1;a=a2,b=b2");
 * </code></pre>
 *
 * Number of allocations: if it is possible to calculate the input range length in constant time,
 * then the output string is pre-allocated as (input range length) * 16.
 * If that is not enough or not possible, then at most one additional allocation per an element is done.
 * So, example 1 is able to join a range of structures while converting them to string
 * with a single allocation.
 */
template<
    typename InputIter,
    typename RecordSeparator,
    typename ValueStringizer,
    typename = std::enable_if_t<IsIteratorV<InputIter>>
>
void join(
    std::string* out,
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator,
    const ValueStringizer& valueStringizer)
{
    detail::join(out, begin, end, recordSeparator, valueStringizer);
}

template<
    typename Container,
    typename RecordSeparator,
    typename ValueStringizer,
    typename = std::enable_if_t<IsIterableV<Container>>
>
void join(
    std::string* out,
    const Container& container,
    const RecordSeparator& recordSeparator,
    const ValueStringizer& valueStringizer)
{
    detail::join(
        out,
        std::begin(container), std::end(container),
        recordSeparator,
        valueStringizer);
}

template<
    typename InputIter,
    typename RecordSeparator,
    typename = std::enable_if_t<IsIteratorV<InputIter>>
>
void join(
    std::string* out,
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator)
{
    detail::join(out, begin, end, recordSeparator);
}

template<
    typename Container,
    typename RecordSeparator,
    typename = std::enable_if_t<IsIterableV<Container>>
>
void join(
    std::string* out,
    const Container& container,
    const RecordSeparator& recordSeparator)
{
    detail::join(out, std::begin(container), std::end(container), recordSeparator);
}

/**
 * Joins a sequence of values and appends the result to BasicBuffer<char>.
 */
template<
    typename InputIter,
    typename RecordSeparator,
    typename ValueStringizer,
    typename = std::enable_if_t<IsIteratorV<InputIter>>
>
void join(
    BasicBuffer<char>* out,
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator,
    const ValueStringizer& valueStringizer)
{
    detail::join(out, begin, end, recordSeparator, valueStringizer);
}

template<
    typename Container,
    typename RecordSeparator,
    typename ValueStringizer,
    typename = std::enable_if_t<IsIterableV<Container>>
>
void join(
    BasicBuffer<char>* out,
    const Container& container,
    const RecordSeparator& recordSeparator,
    const ValueStringizer& valueStringizer)
{
    detail::join(
        out,
        std::begin(container), std::end(container),
        recordSeparator,
        valueStringizer);
}

template<
    typename InputIter,
    typename RecordSeparator,
    typename = std::enable_if_t<IsIteratorV<InputIter>>
>
void join(
    BasicBuffer<char>* out,
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator)
{
    detail::join(out, begin, end, recordSeparator);
}

template<
    typename Container,
    typename RecordSeparator,
    typename = std::enable_if_t<IsIterableV<Container>>
>
void join(
    BasicBuffer<char>* out,
    const Container& container,
    const RecordSeparator& recordSeparator)
{
    detail::join(out, std::begin(container), std::end(container), recordSeparator);
}

/**
 * Joins a sequence of values and returns the resulting string.
 */
template<
    typename InputIter,
    typename RecordSeparator,
    typename ValueStringizer,
    typename = std::enable_if_t<IsIteratorV<InputIter>>
>
std::string join(
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator,
    const ValueStringizer& valueStringizer)
{
    std::string result;
    detail::join(
        &result,
        begin, end,
        recordSeparator,
        valueStringizer);
    return result;
}

template<
    typename Container,
    typename RecordSeparator,
    typename ValueStringizer,
    typename = std::enable_if_t<IsIterableV<Container>>
>
std::string join(
    const Container& container,
    const RecordSeparator& recordSeparator,
    const ValueStringizer& valueStringizer)
{
    std::string result;
    detail::join(
        &result,
        std::begin(container), std::end(container),
        recordSeparator,
        valueStringizer);
    return result;
}

template<
    typename InputIter,
    typename RecordSeparator,
    typename = std::enable_if_t<IsIteratorV<InputIter>>
>
std::string join(
    InputIter begin, InputIter end,
    const RecordSeparator& recordSeparator)
{
    std::string result;
    detail::join(
        &result,
        begin, end,
        recordSeparator);
    return result;
}

template<
    typename Container,
    typename RecordSeparator,
    typename = std::enable_if_t<IsIterableV<Container>>
>
std::string join(
    const Container& container,
    const RecordSeparator& recordSeparator)
{
    std::string result;
    detail::join(
        &result,
        std::begin(container), std::end(container),
        recordSeparator);
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * Reverses words separated by the given separator.
 * No empty words (trailing or inside) are omitted.
 */
template<typename CharType, typename Separator>
void reverseWordsInplace(
    CharType* str,
    std::size_t size,
    Separator separator)
{
    std::reverse(str, str + size);
    std::size_t prevWordStart = 0;
    for (std::size_t i = 0; i < size; ++i)
    {
        if (str[i] == separator)
        {
            if (i > prevWordStart)
                std::reverse(str + prevWordStart, str + i);
            prevWordStart = i + 1;
        }
    }

    if (prevWordStart < size)
        std::reverse(str + prevWordStart, str + size);
}

/**
 * Reverses words separated by the given separator.
 * No empty words (trailing or inside) are omitted.
 */
template<
    template<typename...> class String, typename CharType,
    typename Separator
>
requires std::is_same_v<CharType*, decltype(std::declval<String<CharType>>().data())>
void reverseWordsInplace(String<CharType>& str, Separator separator)
{
    reverseWordsInplace(str.data(), str.size(), separator);
}

/**
 * E.g., reverseWords("test.example.com", '.') returns "com.example.test".
 * Note: empty words are not removed.
 * E.g., reverseWords(".test.example...com", '.') produces "com...example.test."
 */
template<
    template<typename...> class String, typename CharType,
    typename Separator
>
std::basic_string<CharType> reverseWords(String<CharType> str, Separator separator)
{
    std::basic_string<CharType> reversed(std::move(str));
    reverseWordsInplace(reversed, separator);
    return reversed;
}

//-------------------------------------------------------------------------------------------------

/**
 * Remove excess separators between words in the string. This includes preceding, trailing
 * and embedded separators.
 * E.g., removeExcessSeparators("   Hello  world ", ' ') is turned into "Hello world".
 * @return New str size.
 */
template<typename CharType, typename Separator>
std::size_t removeExcessSeparatorsInplace(
    CharType* s,
    std::size_t size,
    Separator separator)
{
    std::size_t newSize = 0;
    CharType prevCh = separator;
    for (std::size_t i = 0; i < size; ++i)
    {
        if (s[i] == separator && prevCh == separator)
            continue; // Skipping extra separator.
        prevCh = s[i];
        s[newSize++] = s[i];
    }
    if (newSize > 0 && s[newSize - 1] == separator)
        --newSize;

    return newSize;
}

/**
 * Remove excess separators between words in the string. This includes preceding, trailing
 * and embedded separators.
 * E.g., removeExcessSeparators("   Hello  world ", ' ') is turned into "Hello world".
 */
template<
    template<typename...> class String, typename CharType,
    typename Separator
>
requires std::is_same_v<CharType*, decltype(std::declval<String<CharType>>().data())>
void removeExcessSeparatorsInplace(String<CharType>& str, Separator separator)
{
    auto newSize = removeExcessSeparatorsInplace(str.data(), str.size(), separator);
    str.resize(newSize);
}

template<template<typename...> class String, typename CharType, typename Separator>
void removeExcessSeparatorsInplace(String<CharType>&&, Separator separator) = delete;

/**
 * Remove excess separators between words in the string. This includes preceding, trailing
 * and embedded separators.
 * E.g., removeExcessSeparators("   Hello  world ", ' ') is turned into "Hello world".
 */
template<
    template<typename...> class String, typename CharType,
    typename Separator
>
std::basic_string<CharType> removeExcessSeparators(String<CharType> str, Separator separator)
{
    std::basic_string<CharType> result(std::move(str));
    removeExcessSeparatorsInplace(result, separator);
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * Compares two strings case-insensitive.
 * @return 0 if strings are equal, negative value if left < right, positive if left > right.
 */
NX_UTILS_API int stricmp(const std::string_view& left, const std::string_view& right);

//-------------------------------------------------------------------------------------------------

template<typename StringType, typename Suffix>
// requires String<StringType>
bool endsWith(const StringType& str, const Suffix& suffix) = delete;

enum class CaseSensitivity
{
    on,
    off,
};

template<typename StringType>
// requires String<StringType>
bool endsWith(
    const StringType& str,
    const std::string_view& suffix,
    CaseSensitivity caseSensitivity = CaseSensitivity::on)
{
    const auto size = str.size();
    if (size < suffix.size())
        return false;

    if (caseSensitivity == CaseSensitivity::on)
    {
        return std::memcmp(
            str.data() + size - suffix.size(),
            suffix.data(),
            suffix.size()) == 0;
    }
    else
    {
        return stricmp(
            std::string_view(str.data() + size - suffix.size(), suffix.size()),
            suffix) == 0;
    }
}

template<typename StringType>
// requires String<StringType>
bool endsWith(
    const StringType& str,
    char suffix,
    CaseSensitivity caseSensitivity = CaseSensitivity::on)
{
    if (str.empty())
        return false;

    return caseSensitivity == CaseSensitivity::on
        ? str.back() == suffix
        : stricmp(std::string_view(str.data() + str.size() - 1, 1),
            std::string_view(&suffix, 1)) == 0;
}

template<typename StringType>
// requires String<StringType>
bool endsWith(
    const StringType& str,
    const std::string& suffix,
    CaseSensitivity caseSensitivity = CaseSensitivity::on)
{
    return endsWith(str, (std::string_view) suffix, caseSensitivity);
}

template<typename StringType>
// requires String<StringType>
bool endsWith(
    const StringType& str,
    const char* suffix,
    CaseSensitivity caseSensitivity = CaseSensitivity::on)
{
    return endsWith(str, std::string_view(suffix), caseSensitivity);
}

//-------------------------------------------------------------------------------------------------

template<typename StringType, typename Prefix>
// requires String<StringType>
bool startsWith(const StringType& str, const Prefix& prefix) = delete;

template<typename StringType>
// requires String<StringType>
bool startsWith(
    const StringType& str,
    const std::string_view& prefix,
    CaseSensitivity caseSensitivity = CaseSensitivity::on)
{
    const auto size = str.size();
    if (size < prefix.size())
        return false;

    if (caseSensitivity == CaseSensitivity::on)
    {
        return std::memcmp(
            str.data(),
            prefix.data(),
            prefix.size()) == 0;
    }
    else
    {
        return stricmp(
            std::string_view(str.data(), prefix.size()),
            prefix) == 0;
    }
}

template<typename StringType>
// requires String<StringType>
bool startsWith(
    const StringType& str,
    char prefix,
    CaseSensitivity caseSensitivity = CaseSensitivity::on)
{
    if (str.empty())
        return false;

    return caseSensitivity == CaseSensitivity::on
        ? str.front() == prefix
        : stricmp(std::string_view(str.data(), 1), std::string_view(&prefix, 1)) == 0;
}

template<typename StringType>
// requires String<StringType>
bool startsWith(
    const StringType& str,
    const std::string& prefix,
    CaseSensitivity caseSensitivity = CaseSensitivity::on)
{
    return startsWith(str, (std::string_view) prefix, caseSensitivity);
}

template<typename StringType>
// requires String<StringType>
bool startsWith(
    const StringType& str,
    const char* prefix,
    CaseSensitivity caseSensitivity = CaseSensitivity::on)
{
    return startsWith(str, std::string_view(prefix), caseSensitivity);
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

namespace detail {

template <typename OutputString>
OutputString toHex(const std::string_view& str)
{
    static constexpr char kDigits[] = "0123456789abcdef";

    OutputString result;
    result.resize(str.size() * 2);

    std::size_t pos = 0;
    for (auto ch: str)
    {
        result[pos++] = kDigits[(ch >> 4) & 0x0f];
        result[pos++] = kDigits[ch & 0x0f];
    }

    return result;
}

} // namespace detail

inline std::string toHex(const std::string_view& str)
{
    return detail::toHex<std::string>(str);
}

inline BasicBuffer<char> toHex(const BasicBuffer<char>& str)
{
    return BasicBuffer<char>(toHex(std::string_view(str.data(), str.size())));
}

template <typename String>
std::decay_t<String> toHex(
    const String& str,
    std::enable_if_t<IsConvertibleToStringViewV<std::decay_t<String>>>* = nullptr)
{
    return detail::toHex<String>(std::string_view(str.data(), (std::size_t) str.size()));
}

// TODO: #akolesnikov Introduced other overloads after introducing local implementation.
inline std::string fromHex(const std::string_view& str)
{
    // TODO: #akolesnikov Get rid of QByteArray. It will save 2 allocations in this function.
    return QByteArray::fromHex(QByteArray::fromRawData(str.data(), (int) str.size()))
        .toStdString();
}

template<typename Value>
// requires std::is_unsigned_v<Value>
std::string toHex(
    Value value,
    std::enable_if_t<std::is_unsigned_v<Value>>* = nullptr)
{
    static constexpr char kDigits[] = "0123456789abcdef";
    static constexpr std::size_t kMaxHexLen = sizeof(value) * 2;

    char str[kMaxHexLen];

    std::size_t pos = kMaxHexLen;
    for (; value > 0 || pos == kMaxHexLen; value >>= 4)
        str[--pos] = kDigits[value & 0x0f];

    return std::string(str + pos, sizeof(str) - pos);
}

//-------------------------------------------------------------------------------------------------

/**
 * Converts ASCII string to low case. Does not support UTF-8.
 */
template<typename StringType>
// requires String<StringType>
void toLower(
    StringType* str,
    std::enable_if_t<std::is_class_v<std::decay_t<StringType>>>* = nullptr)
{
    std::transform(str->begin(), str->end(), str->begin(), &tolower);
}

template<typename StringType>
// requires String<RealStringType>
auto toLower(
    StringType&& str,
    std::enable_if_t<
        !std::is_same_v<std::decay_t<StringType>, std::string_view> &&
        std::is_class_v<std::decay_t<StringType>>
    >* = nullptr)
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
auto toUpper(
    StringType&& str,
    std::enable_if_t<!std::is_same_v<std::decay_t<StringType>, std::string_view>>* = nullptr)
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

/**
 * Replaces every token in text with replacement.
 * text, token, replacement can be std::string or a std::string_view.
 * @return string with replacement applied.
 */
template<typename String1, typename String2, typename String3>
std::string replace(
    const String1& text, const String2& token, const String3& replacement)
{
    // TODO: #akolesnikov Rewrite when <regex> supports std::string_view.

    struct Helper
    {
        static const std::string& toStdString(const std::string& s) { return s; }
        static std::string toStdString(const std::string_view& s) { return std::string(s); }
        static const char* toStdString(const char* s) { return s; }

        static std::regex buildRegex(const std::string& s) { return std::regex(s); }
        static std::regex buildRegex(const std::string_view& s) { return std::regex(s.data(), s.size()); }
        static std::regex buildRegex(const char* s) { return std::regex(s); }
    };

    return std::regex_replace(
        Helper::toStdString(text),
        Helper::buildRegex(token),
        Helper::toStdString(replacement));
}

//-------------------------------------------------------------------------------------------------

/**
 * Comparator for case-sensitive comparison in STL associative containers.
 * Supports both std::string and std::string_view. Allows specifying std::string_view to find()
 * when dictionary uses std::string as a key type.
 */
struct StringLessTransparentComp
{
    using is_transparent = std::true_type;

    /** Case-independent (ci) compare_less binary function. */
    bool operator() (const std::string& c1, const std::string& c2) const
    {
        return nx::utils::stricmp(c1, c2) < 0;
    }

    //---------------------------------------------------------------------------------------------
    // std::string_view support for lookup.

    bool operator() (const std::string& c1, const std::string_view& c2) const
    {
        return nx::utils::stricmp(c1, c2) < 0;
    }

    bool operator() (const std::string_view& c1, const std::string& c2) const
    {
        return nx::utils::stricmp(c1, c2) < 0;
    }

    //---------------------------------------------------------------------------------------------
    // const char* support for lookup.

    bool operator() (const std::string& c1, const char* c2) const
    {
        return nx::utils::stricmp(c1, c2) < 0;
    }

    bool operator() (const char* c1, const std::string& c2) const
    {
        return nx::utils::stricmp(c1, c2) < 0;
    }
};

//-------------------------------------------------------------------------------------------------

/**
 * Hash for std::unordered_*<std::string, ...> containers that enables find(const std::string_view&).
 */
struct StringHashTransparent
{
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;

    size_t operator()(const char* str) const { return hash_type{}(str); }
    size_t operator()(std::string_view str) const { return hash_type{}(str); }
    size_t operator()(std::string const& str) const { return hash_type{}(str); }
};

struct StringEqualToTransparent
{
    using is_transparent = void;

    constexpr bool operator()(const std::string_view& lhs, const std::string_view& rhs) const
    {
        return lhs == rhs;
    }
};

//-------------------------------------------------------------------------------------------------

/**
 * Comparator for case-insensitive comparison in STL associative containers.
 * Supports both std::string and std::string_view. Allows specifying std::string_view to find()
 * when dictionary uses std::string as a key type.
 */
struct ci_less
{
    using is_transparent = std::true_type;

    /** Case-independent (ci) compare_less binary function. */
    bool operator() (const std::string& c1, const std::string& c2) const
    {
        return nx::utils::stricmp(c1, c2) < 0;
    }

    //---------------------------------------------------------------------------------------------
    // std::string_view support for lookup.

    bool operator() (const std::string& c1, const std::string_view& c2) const
    {
        return nx::utils::stricmp(c1, c2) < 0;
    }

    bool operator() (const std::string_view& c1, const std::string& c2) const
    {
        return nx::utils::stricmp(c1, c2) < 0;
    }

    //---------------------------------------------------------------------------------------------
    // const char* support for lookup.

    bool operator() (const std::string& c1, const char* c2) const
    {
        return nx::utils::stricmp(c1, c2) < 0;
    }

    bool operator() (const char* c1, const std::string& c2) const
    {
        return nx::utils::stricmp(c1, c2) < 0;
    }
};

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
// makes "std::string_view == const char*" ambiguous.

inline bool operator==(const std::string_view& left, const char* right)
{ return left == std::string_view(right); }

inline bool operator==(const char* left, const std::string_view& right)
{ return right == left; }

inline bool operator!=(const char* left, const std::string_view& right)
{ return !(left == right); }

inline bool operator!=(const std::string_view& left, const char* right)
{ return !(left == right); }

#endif // #if defined(QT_CORE_LIB)
