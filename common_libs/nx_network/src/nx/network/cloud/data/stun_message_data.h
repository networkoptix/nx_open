#pragma once

#include <nx/network/buffer.h>
#include <nx/network/stun/message.h>
#include <nx/utils/log/log_message.h>
#include <nx/network/stun/extension/stun_extension_types.h>

namespace nx {
namespace hpm {
namespace api {

/** Helper class for serializing / deserializing STUN messages.
    Contains set of utility methods
*/
class NX_NETWORK_API StunMessageParseHelper
{
public:
    nx::String errorText() const;

protected:
    void setErrorText(nx::String text);

    template<typename AttributeType, typename AttributeValueType>
    bool readAttributeValue(
        const nx::stun::Message& message,
        AttributeValueType* const value)
    {
        return readAttributeValue<AttributeType, AttributeValueType>(
            message,
            AttributeType::TYPE,
            value);
    }

    template<typename AttributeType, typename AttributeValueType>
    bool readAttributeValue(
        const nx::stun::Message& message,
        const int type,
        AttributeValueType* const value)
    {
        const auto attribute = message.getAttribute< AttributeType >(type);
        if (!attribute)
        {
            setErrorText(nx::String("Missing required attribute ") +
                stun::extension::attrs::toString(
                    static_cast<stun::extension::attrs::AttributeType>(type)));
            return false;
        }
        *value = attribute->get();
        return true;
    }

    template<typename AttributeType>
    bool readStringAttributeValue(
        const nx::stun::Message& message,
        nx::String* const value)
    {
        const auto attribute = message.getAttribute< AttributeType >();
        if (!attribute)
        {
            setErrorText(nx::String("Missing required attribute ") +
                stun::extension::attrs::toString(AttributeType::TYPE));
            return false;
        }
        *value = attribute->getString();
        return true;
    }

    template<typename AttributeType>
    bool readIntAttributeValue(
        const nx::stun::Message& message,
        int* const value)
    {
        const auto attribute = message.getAttribute< AttributeType >();
        if (!attribute)
        {
            setErrorText(nx::String("Missing required attribute ") +
                stun::extension::attrs::toString(AttributeType::TYPE));
            return false;
        }
        //TODO #ak why getString???
        *value = attribute->getString().toInt();
        return true;
    }

    bool readAttributeValue(
        const nx::stun::Message& message,
        const int type,
        int* const value)
    {
        const auto attribute = message.getAttribute< stun::attrs::IntAttribute >(type);
        if (!attribute)
        {
            setErrorText(nx::String("Missing required attribute ") +
                stun::extension::attrs::toString(static_cast<stun::extension::attrs::AttributeType>(type)));
            return false;
        }
        *value = attribute->value();
        return true;
    }

    bool readAttributeValue(
        const nx::stun::Message& message,
        const int type,
        bool* const value)
    {
        const auto attribute = message.getAttribute< stun::attrs::IntAttribute >(type);
        if (!attribute)
        {
            setErrorText(nx::String("Missing required attribute ") +
                stun::extension::attrs::toString(static_cast<stun::extension::attrs::AttributeType>(type)));
            return false;
        }
        *value = attribute->value() > 0;
        return true;
    }

    /** read attribute value as a std::chrono::duration.
        \note Currently, maximum value of period is limited to max value of int
    */
    template<typename Rep, typename Period>
    bool readAttributeValue(
        const nx::stun::Message& message,
        const int type,
        std::chrono::duration<Rep, Period>* const value)
    {
        const auto attribute = message.getAttribute< stun::attrs::IntAttribute >(type);
        if (!attribute)
        {
            setErrorText(nx::String("Missing required attribute ") +
                stun::extension::attrs::toString(static_cast<stun::extension::attrs::AttributeType>(type)));
            return false;
        }
        *value = std::chrono::duration<Rep, Period>(attribute->value());
        return true;
    }

    template<typename Attribute>
    void readAttributeValue(
        const nx::stun::Message& message,
        const int type,
        Attribute* value,
        Attribute defaultValue)
    {
        if (!readAttributeValue(message, type, value))
            *value = std::move(defaultValue);
    }

    template<typename AttributeType, typename EnumType>
    bool readEnumAttributeValue(
        const nx::stun::Message& message,
        EnumType* const value)
    {
        const auto attribute = message.getAttribute< AttributeType >();
        if (!attribute)
        {
            setErrorText(nx::String("Missing required attribute ") +
                stun::extension::attrs::toString(AttributeType::TYPE));
            return false;
        }
        *value = static_cast<EnumType>(attribute->value());
        return true;
    }

    template<typename EnumType>
    bool readEnumAttributeValue(
        const nx::stun::Message& message,
        const int type,
        EnumType* const value)
    {
        const auto attribute = message.getAttribute< stun::attrs::IntAttribute >(type);
        if (!attribute)
        {
            setErrorText(nx::String("Missing required attribute ") +
                stun::extension::attrs::toString(static_cast<stun::extension::attrs::AttributeType>(type)));
            return false;
        }
        *value = static_cast<EnumType>(attribute->value());
        return true;
    }

    template<typename AttributeType>
    bool readUuidAttributeValue(
        const nx::stun::Message& message,
        QnUuid* const value)
    {
        const auto attribute = message.getAttribute< AttributeType >();
        if (!attribute)
        {
            setErrorText(nx::String("Missing required attribute ") +
                stun::extension::attrs::toString(AttributeType::TYPE));
            return false;
        }
        *value = QnUuid::fromStringSafe(attribute->getString());
        return true;
    }

private:
    nx::String m_text;
};

/** base class for data structure which uses only STUN message attributes */
class NX_NETWORK_API StunMessageAttributesData
:
    public StunMessageParseHelper
{
public:
    virtual ~StunMessageAttributesData();

    virtual void serializeAttributes(nx::stun::Message* const message) = 0;
    virtual bool parseAttributes(const nx::stun::Message& message) = 0;
};

class NX_NETWORK_API StunRequestData
:
    public StunMessageAttributesData
{
public:
    StunRequestData(int method);

    /** fills in all message header and calls \a StunRequestData::serializeAttributes */
    void serialize(nx::stun::Message* const message);
    /** validates message header and calls \a StunRequestData::parseAttributes */
    bool parse(const nx::stun::Message& message);

private:
    int m_method;
};

class NX_NETWORK_API StunResponseData
:
    public StunMessageAttributesData
{
public:
    StunResponseData(int method);

    /** fills in all message header and calls \a StunResponseData::serializeAttributes.
        \note Sets \a messageClass to \a successResponse
        \warning Does not add tansactionId since it MUST match one in request
    */
    void serialize(nx::stun::Message* const message);
    /** validates message header and calls \a StunResponseData::parseAttributes */
    bool parse(const nx::stun::Message& message);

private:
    int m_method;
};

class NX_NETWORK_API StunIndicationData
:
    public StunMessageAttributesData
{
public:
    StunIndicationData(int method);

    /** fills in all message header and calls \a StunResponseData::serializeAttributes.
        \note Sets \a messageClass to \a indication
    */
    void serialize(nx::stun::Message* const message);
    /** validates message header and calls \a StunResponseData::parseAttributes */
    bool parse(const nx::stun::Message& message);

private:
    int m_method;
};

} // namespace api
} // namespace hpm
} // namespace nx
