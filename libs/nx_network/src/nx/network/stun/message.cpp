// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message.h"

#include <mutex>
#include <optional>
#include <random>

#include <openssl/hmac.h>

#include <nx/utils/auth/utils.h>
#include <nx/utils/random.h>
#include <nx/utils/uuid.h>
#include <nx/utils/string.h>
#include <nx/utils/type_utils.h>

#include "message_parser.h"
#include "message_serializer.h"
#include "stun_message_parser_buffer.h"
#include "stun_message_serializer_buffer.h"

static constexpr size_t DEFAULT_BUFFER_SIZE = 4 * 1024;

namespace nx {
namespace network {
namespace stun {

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

static Buffer hmacSha1(const std::string& key, const Message* message)
{
    Buffer buffer;
    buffer.reserve(DEFAULT_BUFFER_SIZE);

    size_t bytes;
    MessageSerializer serializer;
    serializer.setAlwaysAddFingerprint(false);
    serializer.setMessage(message);
    if (serializer.serialize(&buffer, &bytes) != nx::network::server::SerializerState::done)
    {
        NX_ASSERT(false);
    }

    return nx::utils::auth::hmacSha1(key, buffer);
}

void Message::insertIntegrity(const std::string& userName, const std::string& key)
{
    newAttribute< attrs::UserName >(userName);
    if (attributes.count(attrs::nonce) == 0)
        newAttribute< attrs::Nonce >(nx::Buffer(QnUuid::createUuid().toSimpleByteArray()));

    newAttribute< attrs::MessageIntegrity >(nx::Buffer(
        attrs::MessageIntegrity::SIZE, 0));

    // INTEGRITY is calculated without fingerprint.
    bool hadFingerPrint = false;
    if (attributes.count(attrs::fingerPrint) > 0)
    {
        hadFingerPrint = true;
        attributes.erase(attrs::fingerPrint);
    }

    const auto hmac = hmacSha1(key, this);
    NX_ASSERT(hmac.size() == attrs::MessageIntegrity::SIZE);
    newAttribute< attrs::MessageIntegrity >(hmac);

    if (hadFingerPrint)
        newAttribute<attrs::FingerPrint>();
}

bool Message::verifyIntegrity(const std::string& userName, const std::string& key)
{
    const auto userNameAttr = getAttribute< attrs::UserName >();
    if (!userNameAttr || userNameAttr->getString() != userName)
        return false;

    const auto miAttr = getAttribute< attrs::MessageIntegrity >();
    if (!miAttr)
        return false;

    Buffer messageHmac = miAttr->getBuffer();
    newAttribute< attrs::MessageIntegrity >();

    Buffer realHmac = hmacSha1(key, this);
    return messageHmac == realHmac;
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

Message::Message(Header header_, AttributesMap attributes_)
    : header(std::move(header_))
    , attributes(std::move(attributes_))
{
}

void Message::addAttribute(AttributePtr&& attribute)
{
    attributes[attribute->getType()] = std::move(attribute);
}

void Message::clear()
{
    attributes.clear();
}

} // namespace stun
} // namespace network
} // namespace nx
