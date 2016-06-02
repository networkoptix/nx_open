/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_DATA_H
#define STUN_MESSAGE_DATA_H

#include <nx/network/buffer.h>
#include <nx/network/stun/message.h>
#include <nx/utils/log/log_message.h>
#include <nx/network/stun/cc/custom_stun.h>

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API StunMessageData
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
        const auto attribute = message.getAttribute< AttributeType >();
        if (!attribute)
        {
            setErrorText(nx::String("Missing required attribute ") +
                stun::cc::attrs::toString(AttributeType::TYPE));
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
                stun::cc::attrs::toString(AttributeType::TYPE));
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
                stun::cc::attrs::toString(AttributeType::TYPE));
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
                stun::cc::attrs::toString(static_cast<stun::cc::attrs::AttributeType>(type)));
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
                stun::cc::attrs::toString(static_cast<stun::cc::attrs::AttributeType>(type)));
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
                stun::cc::attrs::toString(static_cast<stun::cc::attrs::AttributeType>(type)));
            return false;
        }
        *value = std::chrono::duration<Rep, Period>(attribute->value());
        return true;
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
                stun::cc::attrs::toString(AttributeType::TYPE));
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
                stun::cc::attrs::toString(AttributeType::TYPE));
            return false;
        }
        *value = QnUuid::fromStringSafe(attribute->getString());
        return true;
    }

private:
    nx::String m_text;
};

} // namespace api
} // namespace hpm
} // namespace nx

#endif  //STUN_MESSAGE_DATA_H
