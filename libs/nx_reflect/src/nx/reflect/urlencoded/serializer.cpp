// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "serializer.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace nx::reflect::urlencoded::detail {

void UrlencodedComposer::startArray()
{
    if (!m_stateStack.empty())
    {
        if (m_stateStack.top() == ComposerState::object)
            m_resultStr += m_curKey + "=[";
        if (m_stateStack.top() == ComposerState::array)
            m_resultStr += "[";
    }

    m_stateStack.push(ComposerState::array);
}

void UrlencodedComposer::endArray(int /*items*/)
{
    if (!m_stateStack.empty())
        m_stateStack.pop();
    if (*m_resultStr.rbegin() == ',')
        *m_resultStr.rbegin() = ']';
    else
        m_resultStr += "]";
    addFieldSuffix();
}

void UrlencodedComposer::startObject()
{
    if (!m_stateStack.empty())
    {
        if (m_stateStack.top() == ComposerState::object)
            m_resultStr += m_curKey + "={";
        if (m_stateStack.top() == ComposerState::array)
            m_resultStr += "{";
    }

    m_stateStack.push(ComposerState::object);
}

void UrlencodedComposer::endObject(int /*members*/)
{
    if (!m_stateStack.empty())
        m_stateStack.pop();
    m_resultStr = m_resultStr.substr(0, m_resultStr.length() - 1);
    if (!m_stateStack.empty())
        m_resultStr += "}";
    addFieldSuffix();
}

void UrlencodedComposer::writeBool(bool val)
{
    addFieldPrefix();
    m_resultStr += val ? "true" : "false";
    addFieldSuffix();
}

void UrlencodedComposer::writeInt(const std::int64_t& val)
{
    addFieldPrefix();
    m_resultStr += std::to_string(val);
    addFieldSuffix();
}

void UrlencodedComposer::writeFloat(const double& val)
{
    addFieldPrefix();
    m_resultStr += std::to_string(val);
    addFieldSuffix();
}

void UrlencodedComposer::writeString(const std::string_view& val)
{
    addFieldPrefix();
    m_resultStr += encode(val.data());
    addFieldSuffix();
}

void UrlencodedComposer::writeRawString(const std::string_view& val)
{
    addFieldPrefix();
    m_resultStr += val.data();
    addFieldSuffix();
}

void UrlencodedComposer::writeNull()
{
}

void UrlencodedComposer::writeAttributeName(const std::string_view& name)
{
    m_curKey = encode(name);
}

std::string UrlencodedComposer::encode(const std::string_view& str)
{
    std::ostringstream result;
    result.fill('0');
    result << std::hex;
    for (auto c: str)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            result << c;
        else
        {
            result << std::uppercase;
            result << '%' << std::setw(2) << int((unsigned char) c);
            result << std::nouppercase;
        }
    }
    return result.str();
}

void UrlencodedComposer::addFieldPrefix()
{
    if (!m_stateStack.empty() && (m_stateStack.top() == ComposerState::object))
        m_resultStr += m_curKey + "=";
}

void UrlencodedComposer::addFieldSuffix()
{
    if (m_stateStack.empty())
        return; // we are in custom serialize
    if (m_stateStack.top() == ComposerState::object)
        m_resultStr += '&';
    else
        m_resultStr += ',';
}

std::string UrlencodedComposer::take()
{
    return m_resultStr;
}

} // namespace nx::reflect::urlencoded::detail
