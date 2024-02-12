// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message.h"

#include <mutex>
#include <optional>
#include <random>

#include <openssl/hmac.h>

#include <nx/utils/random.h>
#include <nx/utils/uuid.h>
#include <nx/utils/string.h>
#include <nx/utils/type_utils.h>

#include "message_parser.h"
#include "message_serializer.h"
#include "stun_message_parser_buffer.h"
#include "stun_message_serializer_buffer.h"

namespace nx::network::stun {

const char* toString(MessageClass value)
{
    switch (value)
    {
        case MessageClass::request:
            return "request";
        case MessageClass::indication:
            return "indication";
        case MessageClass::successResponse:
            return "successResponse";
        case MessageClass::errorResponse:
            return "errorResponse";
        default:
            return "unknown";
    }
}

//-------------------------------------------------------------------------------------------------

Header::Header():
    messageClass(MessageClass::unknown),
    method(MethodType::invalid),
    transactionId(nullTransactionId)
{
}

Header::Header(const Header& right):
    messageClass(right.messageClass),
    method(right.method),
    transactionId(right.transactionId)
{
}

Header::Header(Header&& right):
    messageClass(std::move(right.messageClass)),
    method(std::move(right.method)),
    transactionId(std::move(right.transactionId))
{
}

Header::Header(MessageClass messageClass_, int method_):
    messageClass(messageClass_),
    method(method_),
    transactionId(makeTransactionId())
{
}

Header::Header(MessageClass messageClass_, int method_, Buffer transactionId_):
    messageClass(messageClass_),
    method(method_),
    transactionId(std::move(transactionId_))
{
}

Header& Header::operator=(Header&& rhs)
{
    if (this != &rhs)
    {
        messageClass = std::move(rhs.messageClass);
        method = std::move(rhs.method);
        transactionId = std::move(rhs.transactionId);
    }
    return *this;
}

std::string Header::toString() const
{
    return nx::format("class %1, method %2, transaction id %3")
        .args(messageClass, method, nx::utils::toHex(transactionId)).toStdString();
}

Buffer Header::makeTransactionId()
{
    return nx::utils::random::generate<Buffer>(TRANSACTION_ID_SIZE);
}

Buffer Header::nullTransactionId(TRANSACTION_ID_SIZE, 0);

std::size_t Message::attributeCount() const
{
    return m_attributes.size();
}

void Message::eraseAttribute(int type)
{
    auto it = findAttr(type);
    if (it != m_attributes.end())
        m_attributes.erase(it);
}

void Message::eraseAllAttributes()
{
    m_attributes.clear();
}

void Message::insertIntegrity(const std::string& userName, const std::string& key)
{
    newAttribute< attrs::UserName >(userName);
    if (findAttr(attrs::nonce) == m_attributes.end())
        newAttribute< attrs::Nonce >(nx::Buffer(nx::Uuid::createUuid().toSimpleByteArray()));

    newAttribute< attrs::MessageIntegrity >(nx::Buffer(
        attrs::MessageIntegrity::SIZE, 0));

    const auto hmac = calcMessageIntegrity(*this, key, {});
    NX_ASSERT(hmac.size() == attrs::MessageIntegrity::SIZE);
    newAttribute< attrs::MessageIntegrity >(hmac);
}

bool Message::verifyIntegrity(
    const std::string& userName,
    const std::string& key,
    MessageIntegrityOptions options) const
{
    const auto userNameAttr = getAttribute< attrs::UserName >();
    if (!userNameAttr || userNameAttr->getString() != userName)
        return false;

    const auto miAttr = getAttribute< attrs::MessageIntegrity >();
    if (!miAttr)
        return false;

    const Buffer receivedHmac = miAttr->getBuffer();
    const Buffer calculatedHmac = calcMessageIntegrity(*this, key, options);
    return receivedHmac == calculatedHmac;
}

std::optional<std::string> Message::hasError(SystemError::ErrorCode code) const
{
    if (code != SystemError::noError)
    {
        return nx::format("System error %1: %2")
            .args(code, SystemError::toString(code)).toStdString();
    }

    if (header.messageClass != MessageClass::successResponse)
    {
        if (const auto err = getAttribute< attrs::ErrorCode >())
        {
            return nx::format("STUN error %1: %2")
                .args(err->getCode(), err->getString()).toStdString();
        }
        else
        {
            return "STUN error without ErrorCode";
        }
    }

    return std::nullopt;
}

Message::Message(Header header_):
    header(std::move(header_))
{
}

void Message::addAttribute(AttributePtr&& attribute)
{
    const auto type = attribute->getType();
    auto it = findAttr(type);
    if (it == m_attributes.end())
        m_attributes.push_back(std::move(attribute));
    else
        *it = std::move(attribute);
}

void Message::clear()
{
    m_attributes.clear();
}

Message::Attributes::iterator Message::findAttr(int type)
{
    return std::find_if(
        m_attributes.begin(), m_attributes.end(),
        [type](const auto& attr) { return attr->getType() == type; });
}

Message::Attributes::const_iterator Message::findAttr(int type) const
{
    return std::find_if(
        m_attributes.begin(), m_attributes.end(),
        [type](const auto& attr) { return attr->getType() == type; });
}

} // namespace nx::network::stun
