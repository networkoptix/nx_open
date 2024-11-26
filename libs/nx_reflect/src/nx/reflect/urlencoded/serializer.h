// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stack>
#include <string>

#include <nx/reflect/basic_serializer.h>
#include <nx/reflect/type_utils.h>

namespace nx::reflect::urlencoded {

namespace detail {

class NX_REFLECT_API UrlencodedComposer: public AbstractComposer<std::string>
{
    enum class ComposerState
    {
        array,
        object
    };

public:
    virtual void startArray() override;
    virtual void endArray(int items) override;
    virtual void startObject() override;
    virtual void endObject(int members) override;
    virtual void writeBool(bool val) override;
    virtual void writeInt(const std::int64_t& val) override;
    virtual void writeFloat(const double& val) override;
    virtual void writeString(const std::string_view& val) override;
    virtual void writeRawString(const std::string_view& val) override;
    virtual void writeNull() override;
    virtual void writeAttributeName(const std::string_view& name) override;

    virtual std::string take() override;

    static std::string encode(const std::string_view& str);

private:
    std::string m_resultStr;
    std::stack<ComposerState> m_stateStack;
    std::string m_curKey;

private:
    void addFieldPrefix();
    void addFieldSuffix();
};

struct SerializationContext
{
    UrlencodedComposer composer;

    template<typename T>
    int beforeSerialize(const T&) { return -1; /*dummy*/ }

    template<typename T>
    void afterSerialize(const T&, int) {}
};

} // namespace detail

using SerializationContext = detail::SerializationContext;

template<typename Data>
void serialize(SerializationContext* ctx, const Data& data)
{
    BasicSerializer::serializeAdl(ctx, data);
}

/**
 * Serializes object of supported type Data to urlencoded format.
 * @param data has to be instrumented type or build-in type
 */
template<typename Data>
std::string serialize(const Data& data)
{
    SerializationContext ctx;
    serialize(&ctx, data);
    return ctx.composer.take();
}

} // namespace nx::reflect::urlencoded
