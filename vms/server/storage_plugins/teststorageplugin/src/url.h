#pragma once

#include <string>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <common.h>

namespace utils {

using ParamsMap = std::unordered_map<std::string, std::string>;

class NX_TEST_STORAGE_PLUGIN_API Url
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
    std::string path() const;

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
    std::string m_host;
    std::string m_path;
    ParamsMap m_params;
    size_t m_index;
    ParseState m_state;
};

}

