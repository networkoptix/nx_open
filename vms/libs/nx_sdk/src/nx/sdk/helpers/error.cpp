// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "error.h"

namespace nx {
namespace sdk {

Error::Error():
    m_message(makePtr<String>()),
    m_details(makePtr<StringMap>())
{
}

Error::Error(ErrorCode errorCode, const char* message):
    m_errorCode(errorCode),
    m_message(makePtr<String>(message))
{
}

void Error::setError(ErrorCode errorCode, const char* message)
{
    m_errorCode = errorCode;
    m_message->setString(message);
}

void Error::setDetail(const char* key, const char* message)
{
    m_details->addItem(key, message);
}

ErrorCode Error::errorCode() const
{
    return m_errorCode;
}

const IString* Error::message() const
{
    m_message->addRef();
    return m_message.get();
}

const IStringMap* Error::details() const
{
    m_details->addRef();
    return m_details.get();
}

} // namespace sdk
} // namespace nx
