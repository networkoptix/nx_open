#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

nx::String StunMessageParseHelper::errorText() const
{
    return m_text;
}

void StunMessageParseHelper::setErrorText(nx::String text)
{
    m_text = std::move(text);
}


/** StunMessageAttributesData */

StunMessageAttributesData::~StunMessageAttributesData()
{
}

/** StunRequestData */

StunRequestData::StunRequestData(int method):
    m_method(method)
{
}

void StunRequestData::serialize(nx::stun::Message* const message)
{
    message->header = stun::Header(stun::MessageClass::request, m_method);
    serializeAttributes(message);
}

bool StunRequestData::parse(const nx::stun::Message& message)
{
    if (message.header.messageClass != stun::MessageClass::request ||
        message.header.method != m_method)
    {
        return false;
    }
    return parseAttributes(message);
}


/** StunResponseData */

StunResponseData::StunResponseData(int method):
    m_method(method)
{
}

void StunResponseData::serialize(nx::stun::Message* const message)
{
    message->header.messageClass = stun::MessageClass::successResponse;
    message->header.method = m_method;
    serializeAttributes(message);
}

bool StunResponseData::parse(const nx::stun::Message& message)
{
    //NOTE: not checking messageClass since response may have different values
    if (message.header.method != m_method)
        return false;
    return parseAttributes(message);
}


/** StunIndicationData */

StunIndicationData::StunIndicationData(int method):
    m_method(method)
{
}

void StunIndicationData::serialize(nx::stun::Message* const message)
{
    message->header = stun::Header(stun::MessageClass::indication, m_method);
    serializeAttributes(message);
}

bool StunIndicationData::parse(const nx::stun::Message& message)
{
    if (message.header.messageClass != stun::MessageClass::indication ||
        message.header.method != m_method)
    {
        return false;
    }
    return parseAttributes(message);
}

} // namespace api
} // namespace hpm
} // namespace nx
