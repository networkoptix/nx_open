#pragma once

#include <functional>
#include <string>
#include <boost/optional.hpp>

enum class Result
{
    ok,
    invalidArg,
    execFailed
};

inline std::string toString(Result result)
{
    switch (result)
    {
        case Result::ok: return "ok";
        case Result::invalidArg: return "invalid arguments";
        case Result::execFailed: return "execution failed";
    }

    assert(false);
    return "";
}

using Action = std::function<Result(const std::string&, int)>;

inline bool isNotSpace(char c) { return !isspace(c); }
inline bool isNotQuote(char c) { return c != '\''; }

template<typename Pred>
void advanceWhile(std::string::const_iterator* it, std::string::const_iterator end, Pred pred)
{
    while (*it != end && pred(**it))
        ++(*it);
}

template<typename OutBuf>
void extractWord(std::string::const_iterator* it, std::string::const_iterator end, OutBuf* outBuf)
{
    std::function<bool(char)> pred = &isNotSpace;
    if (**it == '\'')
    {
        ++(*it);
        pred = &isNotQuote;
    }
    auto wordStartIt = *it;
    advanceWhile(it, end, pred);
    std::string result(wordStartIt, *it);
    ++(*it);
    *outBuf = result;
}

template<typename... Tail>
bool parseCommandImpl(std::string::const_iterator* it, std::string::const_iterator end,
    std::string* head, Tail... tail);

template<typename... Tail>
bool parseCommandImpl(std::string::const_iterator* it, std::string::const_iterator end,
    boost::optional<std::string>* head, Tail... tail);

inline bool parseCommandImpl(std::string::const_iterator* /*it*/, std::string::const_iterator /*end*/)
{
    return true;
}

template<typename... Tail>
bool parseCommandImpl(std::string::const_iterator* it, std::string::const_iterator end,
    boost::optional<std::string>* head, Tail... tail)
{
    advanceWhile(it, end, &isspace);
    if (*it != end)
        extractWord(it, end, head);

    return parseCommandImpl(it, end, tail...);
}

template<typename... Tail>
bool parseCommandImpl(std::string::const_iterator* it, std::string::const_iterator end,
    std::string* head, Tail... tail)
{
    advanceWhile(it, end, &isspace);
    if (*it == end)
        return false;

    extractWord(it, end, head);
    return parseCommandImpl(it, end, tail...);
}

template<typename... Args>
bool parseCommand(const std::string& command, Args... args)
{
    auto it = command.cbegin();
    advanceWhile(&it, command.cend(), &isspace);
    advanceWhile(&it, command.cend(), &isNotSpace); //< Skipping command name.
    return parseCommandImpl(&it, command.cend(), args...);
}
