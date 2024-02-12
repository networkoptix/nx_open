// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/network/stun/message.h>
#include <nx/utils/buffer.h>
#include <nx/utils/log/format.h>
#include <nx/utils/string.h>

namespace nx::hpm::api {

/**
 * Helper class for serializing / deserializing STUN messages.
 * Contains set of utility methods.
 */
struct NX_NETWORK_API StunMessageParseHelper
{
public:
    std::string errorText() const;

protected:
    void setErrorText(std::string text);

    template<typename AttributeType, typename AttributeValueType>
    bool readAttributeValue(
        const nx::network::stun::Message& message,
        AttributeValueType* const value)
    {
        return readAttributeValue<AttributeType, AttributeValueType>(
            message,
            AttributeType::TYPE,
            value);
    }

    template<typename AttributeType, typename AttributeValueType>
    bool readAttributeValue(
        const nx::network::stun::Message& message,
        const int type,
        AttributeValueType* const value)
    {
        const auto attribute = message.getAttribute< AttributeType >(type);
        if (!attribute)
        {
            setErrorText(nx::utils::buildString(
                "Missing required attribute ",
                network::stun::extension::attrs::toString(
                    static_cast<network::stun::extension::attrs::AttributeType>(type))));
            return false;
        }
        *value = attribute->get();
        return true;
    }

    template<typename AttributeType>
    bool readStringAttributeValue(
        const nx::network::stun::Message& message,
        std::string* const value)
    {
        const auto attribute = message.getAttribute< AttributeType >();
        if (!attribute)
        {
            setErrorText(nx::utils::buildString("Missing required attribute ",
                network::stun::extension::attrs::toString(AttributeType::TYPE)));
            return false;
        }
        *value = attribute->getString();
        return true;
    }

    template<typename AttributeType>
    bool readIntAttributeValue(
        const nx::network::stun::Message& message,
        int* const value)
    {
        const auto attribute = message.getAttribute< AttributeType >();
        if (!attribute)
        {
            setErrorText(nx::utils::buildString("Missing required attribute ",
                network::stun::extension::attrs::toString(AttributeType::TYPE)));
            return false;
        }
        //TODO #akolesnikov why getString???
        *value = nx::utils::stoi(attribute->getString());
        return true;
    }

    bool readAttributeValue(
        const nx::network::stun::Message& message,
        const int type,
        int* const value)
    {
        const auto attribute = message.getAttribute< network::stun::attrs::IntAttribute >(type);
        if (!attribute)
        {
            setErrorText(nx::utils::buildString(
                "Missing required attribute ",
                network::stun::extension::attrs::toString(
                    static_cast<network::stun::extension::attrs::AttributeType>(type))));
            return false;
        }
        *value = attribute->value();
        return true;
    }

    bool readAttributeValue(
        const nx::network::stun::Message& message,
        const int type,
        bool* const value)
    {
        const auto attribute = message.getAttribute< network::stun::attrs::IntAttribute >(type);
        if (!attribute)
        {
            setErrorText(nx::utils::buildString(
                "Missing required attribute ",
                network::stun::extension::attrs::toString(
                    static_cast<network::stun::extension::attrs::AttributeType>(type))));
            return false;
        }
        *value = attribute->value() > 0;
        return true;
    }

    /** read attribute value as a std::chrono::duration.
        NOTE: Currently, maximum value of period is limited to max value of int
    */
    template<typename Rep, typename Period>
    bool readAttributeValue(
        const nx::network::stun::Message& message,
        const int type,
        std::chrono::duration<Rep, Period>* const value,
        typename std::enable_if<std::is_arithmetic<Rep>::value>::type* = nullptr)
    {
        const auto attribute = message.getAttribute< network::stun::attrs::IntAttribute >(type);
        if (!attribute)
        {
            setErrorText(nx::utils::buildString(
                "Missing required attribute ",
                network::stun::extension::attrs::toString(
                    static_cast<network::stun::extension::attrs::AttributeType>(type))));
            return false;
        }
        *value = std::chrono::duration<Rep, Period>(attribute->value());
        return true;
    }

    template<typename Attribute>
    void readAttributeValue(
        const nx::network::stun::Message& message,
        const int type,
        Attribute* value,
        Attribute defaultValue)
    {
        if (!readAttributeValue(message, type, value))
            *value = std::move(defaultValue);
    }

    template<typename AttributeType, typename EnumType>
    bool readEnumAttributeValue(
        const nx::network::stun::Message& message,
        EnumType* const value)
    {
        const auto attribute = message.getAttribute< AttributeType >();
        if (!attribute)
        {
            setErrorText(nx::utils::buildString(
                "Missing required attribute ",
                network::stun::extension::attrs::toString(AttributeType::TYPE)));
            return false;
        }
        *value = static_cast<EnumType>(attribute->value());
        return true;
    }

    template<typename EnumType>
    bool readEnumAttributeValue(
        const nx::network::stun::Message& message,
        const int type,
        EnumType* const value)
    {
        const auto attribute = message.getAttribute< network::stun::attrs::IntAttribute >(type);
        if (!attribute)
        {
            setErrorText(nx::utils::buildString(
                "Missing required attribute ",
                network::stun::extension::attrs::toString(
                    static_cast<network::stun::extension::attrs::AttributeType>(type))));
            return false;
        }
        *value = static_cast<EnumType>(attribute->value());
        return true;
    }

    template<typename AttributeType>
    bool readUuidAttributeValue(
        const nx::network::stun::Message& message,
        nx::Uuid* const value)
    {
        const auto attribute = message.getAttribute< AttributeType >();
        if (!attribute)
        {
            setErrorText(nx::utils::buildString(
                "Missing required attribute ",
                network::stun::extension::attrs::toString(AttributeType::TYPE)));
            return false;
        }
        *value = nx::Uuid::fromStringSafe(attribute->getString());
        return true;
    }

private:
    std::string m_text;
};

/** Base class for data structure which uses only STUN message attributes. */
struct NX_NETWORK_API StunMessageAttributesData:
    StunMessageParseHelper
{
    virtual ~StunMessageAttributesData();

    virtual void serializeAttributes(nx::network::stun::Message* const message) = 0;
    virtual bool parseAttributes(const nx::network::stun::Message& message) = 0;
};

struct NX_NETWORK_API StunRequestData:
    StunMessageAttributesData
{
    StunRequestData(int method);

    /** Fills in all message header and calls StunRequestData::serializeAttributes. */
    void serialize(nx::network::stun::Message* const message);
    /** Validates message header and calls StunRequestData::parseAttributes. */
    bool parse(const nx::network::stun::Message& message);

private:
    int m_method;
};

struct NX_NETWORK_API StunResponseData:
    StunMessageAttributesData
{
    StunResponseData(int method);

    /**
     * Fills in all message header and calls StunResponseData::serializeAttributes.
     * NOTE: Sets messageClass to successResponse.
     * WARNING: Does not add tansactionId since it MUST match one in request.
     */
    void serialize(nx::network::stun::Message* const message);
    /** Validates message header and calls StunResponseData::parseAttributes. */
    bool parse(const nx::network::stun::Message& message);

private:
    int m_method;
};

struct NX_NETWORK_API StunIndicationData:
    StunMessageAttributesData
{
    StunIndicationData(int method);

    /**
     * Fills in all message header and calls StunResponseData::serializeAttributes.
     * NOTE: Sets messageClass to indication.
     */
    void serialize(nx::network::stun::Message* const message);
    /** Validates message header and calls StunResponseData::parseAttributes. */
    bool parse(const nx::network::stun::Message& message);

private:
    int m_method;
};

} // namespace nx::hpm::api
