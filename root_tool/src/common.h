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

    return "";
}

using Action = std::function<Result(const std::string&, int)>;

inline bool skipWhileSpacesPred(char c) { return isspace(c); }
inline bool skipWhileNotSpacesPred(char c) { return !isspace(c); }
inline bool skipWhileNotQuotePred(char c) { return c != '\''; }

template<typename Pred>
void advanceIt(std::string::const_iterator& it, std::string::const_iterator end, Pred pred)
{
    while (it != end && pred(*it))
        ++it;
}

template<typename OutBuf>
void extractWord(std::string::const_iterator& it, std::string::const_iterator end, OutBuf* outBuf)
{
    std::function<bool(char)> pred = &skipWhileNotSpacesPred;
    if (*it == '\'')
    {
        ++it;
        pred = &skipWhileNotQuotePred;
    }
    auto wordStartIt = it;
    advanceIt(it, end, pred);
    std::string result(wordStartIt, it);
    ++it;
    *outBuf = result;
}

template<typename... Tail>
bool parseCommandImpl(std::string::const_iterator& it, std::string::const_iterator end,
    std::string* head, Tail... tail);

template<typename... Tail>
bool parseCommandImpl(std::string::const_iterator& it, std::string::const_iterator end,
    boost::optional<std::string>* head, Tail... tail);

inline bool parseCommandImpl(std::string::const_iterator& /*it*/, std::string::const_iterator /*end*/)
{
    return true;
}

template<typename... Tail>
bool parseCommandImpl(std::string::const_iterator& it, std::string::const_iterator end,
    boost::optional<std::string>* head, Tail... tail)
{
    advanceIt(it, end, &skipWhileSpacesPred);
    if (it != end)
        extractWord(it, end, head);

    return parseCommandImpl(it, end, tail...);
}

template<typename... Tail>
bool parseCommandImpl(std::string::const_iterator& it, std::string::const_iterator end,
    std::string* head, Tail... tail)
{
    advanceIt(it, end, &skipWhileSpacesPred);
    if (it == end)
        return false;

    extractWord(it, end, head);
    return parseCommandImpl(it, end, tail...);
}

template<typename... Args>
bool parseCommand(const std::string& command, Args... args)
{
    auto it = command.cbegin();
    advanceIt(it, command.cend(), &skipWhileSpacesPred);
    advanceIt(it, command.cend(), &skipWhileNotSpacesPred); //< Skipping command name.
    return parseCommandImpl(it, command.cend(), args...);
}
