#pragma once

#include <string>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <algorithm>

namespace utils {

using ParamsMap = std::unordered_map<std::string, std::string>;

class Url
{
    enum class ParseState
    {
        scheme,
        hostPath,
        params,
        nextParam,
        error,
        ok
    };

public:
    Url(const std::string& url);

    std::string scheme() const;
    std::string url() const;
    std::string hostPath() const;
    std::string host() const;

    ParamsMap params() const;
    bool valid() const;

private:
    void parse();
    ParseState parseSome();
    ParseState parseScheme();
    ParseState parseHostPath();
    ParseState parseParams();
    ParseState parseParam();

    void copyAndAdvance(std::string* target, size_t endIndex);
    ParseState checkAndAdvance(const std::string& stringToCheck, ParseState nextState);

private:
    const std::string& m_url;
    std::string m_scheme;
    std::string m_hostPath;
    ParamsMap m_params;
    size_t m_index;
    ParseState m_state;
};

}

