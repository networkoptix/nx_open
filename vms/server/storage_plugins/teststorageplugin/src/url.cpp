#include <iterator>
#include "url.h"

namespace utils {

Url::Url(const std::string& url)
    : m_url(url),
      m_index(0),
      m_state(ParseState::scheme)
{
    parse();
}

std::string Url::scheme() const 
{ 
    return m_scheme; 
}

std::string Url::url() const 
{ 
    return m_scheme + "://" + m_host + m_path; 
}

std::string Url::hostPath() const
{
    return m_host + m_path;
}

std::string Url::path() const
{
    return m_path;
}

std::string Url::host() const
{
    return m_host;
}

ParamsMap Url::params() const 
{ 
    return m_params; 
}

bool Url::valid() const 
{ 
    return m_state == ParseState::ok; 
}

void Url::parse()
{
    while (m_state != ParseState::error && m_state != ParseState::ok)
        m_state = parseSome();
}

Url::ParseState Url::parseSome()
{
    switch(m_state)
    {
    case ParseState::scheme: return parseScheme();
    case ParseState::hostPath: return parseHostPath();
    case ParseState::params: return parseParams();
    default: return ParseState::error;
    }
}

Url::ParseState Url::parseScheme()
{
    auto colonIndex = m_url.find(':');
    if (colonIndex == std::string::npos || colonIndex == m_index)
        return ParseState::error;

    copyAndAdvance(&m_scheme, colonIndex);
    return checkAndAdvance("://", ParseState::hostPath);
}

Url::ParseState Url::parseHostPath()
{
    if (m_url[m_index] == '/')
        return ParseState::error;

    auto pathBeginIndex = m_url.find('/', m_index);
    if (pathBeginIndex == m_index)
        return ParseState::error;

    if (pathBeginIndex == std::string::npos)
    {
        pathBeginIndex = m_url.find('?', m_index);
        if (pathBeginIndex == std::string::npos)
        {
            copyAndAdvance(&m_host, m_url.size());
            return ParseState::ok;
        }
        if (pathBeginIndex == m_index)
            return ParseState::error;

        copyAndAdvance(&m_host, pathBeginIndex);
    }
    else
    {
        copyAndAdvance(&m_host, pathBeginIndex);
        auto paramsBeginIndex = m_url.find('?', m_index);
        if (paramsBeginIndex == m_index)
            return ParseState::error;

        if (paramsBeginIndex == std::string::npos)
        {
            copyAndAdvance(&m_path, m_url.size());
            return ParseState::ok;
        }

        copyAndAdvance(&m_path, paramsBeginIndex);
    }

    return checkAndAdvance("?", ParseState::params);
}

Url::ParseState Url::parseParams()
{
    ParseState state;
    while ((state = parseParam()) == ParseState::nextParam)
        ;

    return state;
}

Url::ParseState Url::parseParam()
{
    std::string key;
    auto equalSignIndex = m_url.find('=', m_index);

    if (equalSignIndex == m_index)
        return ParseState::error;

    if (equalSignIndex == std::string::npos)
    {
        copyAndAdvance(&key, m_url.size());
        m_params[key] = "";
        return ParseState::ok;
    }

    copyAndAdvance(&key, equalSignIndex);
    if (checkAndAdvance("=", ParseState::ok) == ParseState::error)
        return ParseState::error;
    
    std::string value;
    auto ampIndex = m_url.find('&', m_index);

    if (ampIndex == std::string::npos)
    {
        copyAndAdvance(&value, m_url.size());
        m_params[key] = value;
        return ParseState::ok;
    }

    copyAndAdvance(&value, ampIndex);
    m_params[key] = value;

    return checkAndAdvance("&", ParseState::nextParam);
} 

void Url::copyAndAdvance(std::string* target, size_t endIndex)
{
    std::copy(m_url.cbegin() + m_index, 
              m_url.cbegin() + endIndex, 
              std::back_inserter(*target));

    m_index = endIndex;
}

Url::ParseState Url::checkAndAdvance(const std::string& stringToCheck, ParseState nextState)
{
    for (size_t i = 0; i < stringToCheck.size(); ++i)
    {
        if (m_index >= m_url.size() || m_url[m_index] != stringToCheck[i])
            return ParseState::error;
        ++m_index;
    }

    return nextState;
}

}