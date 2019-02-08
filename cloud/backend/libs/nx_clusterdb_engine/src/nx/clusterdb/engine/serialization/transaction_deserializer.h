#pragma once

#include <memory>

#include <nx/fusion/serialization/ubjson_reader.h>

#include "serializable_transaction.h"
#include "ubjson_serialized_transaction.h"
#include "../command.h"

namespace nx::clusterdb::engine {

class DeserializableCommandData;

/**
 * This class is to encapsulate transaction versioning support when relevant.
 */
class TransactionDeserializer
{
public:
    static bool deserialize(
        QnUbjsonReader<QByteArray>* const stream,
        CommandHeader* const transactionHeader,
        int transactionFormatVersion);

    template<typename TransactionData>
    static bool deserialize(
        QnUbjsonReader<QByteArray>* const stream,
        TransactionData* const transactionData,
        int /*transactionFormatVersion*/)
    {
        return QnUbjson::deserialize(stream, transactionData);
    }

    static std::unique_ptr<DeserializableCommandData> deserialize(
        Qn::SerializationFormat dataFormat,
        const std::string& originatorPeerId,
        int commandFormatVersion,
        QByteArray serializedCommand);

private:
    static std::unique_ptr<DeserializableCommandData> deserializeUbjson(
        const QnUuid& moduleGuid,
        int commandFormatVersion,
        QByteArray serializedCommand);

    static std::unique_ptr<DeserializableCommandData> deserializeJson(
        const QnUuid& moduleGuid,
        QByteArray serializedCommand);
};

//-------------------------------------------------------------------------------------------------

/**
 * Wraps raw command data and provides easy-to-use deserilization routine that hides details like
 * different data formats and data versions.
 */
class DeserializableCommandData
{
public:
    DeserializableCommandData(
        CommandHeader commandHeader,
        QJsonObject jsonData);

    DeserializableCommandData(
        CommandHeader commandHeader,
        std::unique_ptr<TransactionUbjsonDataSource> ubjsonData);

    DeserializableCommandData(DeserializableCommandData&&) = default;
    DeserializableCommandData& operator=(DeserializableCommandData&&) = default;

    virtual ~DeserializableCommandData() = default;

    const CommandHeader& header() const;

    template<typename CommandDescriptor>
    std::unique_ptr<SerializableCommand<CommandDescriptor>>
        deserialize(int commandFormatVersion) const;

private:
    CommandHeader m_commandHeader;
    std::optional<QJsonObject> m_jsonData;
    std::unique_ptr<TransactionUbjsonDataSource> m_ubjsonData;

    template<typename CommandDescriptor>
    std::unique_ptr<SerializableCommand<CommandDescriptor>> deserializeJsonData() const;

    template<typename CommandDescriptor>
    std::unique_ptr<SerializableCommand<CommandDescriptor>>
        deserializeUbjsonData(int commandFormatVersion) const;
};

//-------------------------------------------------------------------------------------------------

template<typename CommandDescriptor>
std::unique_ptr<SerializableCommand<CommandDescriptor>>
    DeserializableCommandData::deserialize(int commandFormatVersion) const
{
    NX_ASSERT(m_commandHeader.command == CommandDescriptor::code);

    if (m_jsonData)
        return deserializeJsonData<CommandDescriptor>();
    else if (m_ubjsonData)
        return deserializeUbjsonData<CommandDescriptor>(commandFormatVersion);
    else
        return nullptr;
}

template<typename CommandDescriptor>
std::unique_ptr<SerializableCommand<CommandDescriptor>>
    DeserializableCommandData::deserializeJsonData() const
{
    using SpecificCommand = Command<typename CommandDescriptor::Data>;

    auto command = SpecificCommand(std::move(m_commandHeader));
    if (!QJson::deserialize((*m_jsonData)["params"], &command.params))
        return nullptr;

    return std::make_unique<SerializableCommand<CommandDescriptor>>(
        std::move(command));
}

template<typename CommandDescriptor>
std::unique_ptr<SerializableCommand<CommandDescriptor>>
    DeserializableCommandData::deserializeUbjsonData(int commandFormatVersion) const
{
    using SpecificCommand = Command<typename CommandDescriptor::Data>;

    auto command = SpecificCommand(std::move(m_commandHeader));

    if (!TransactionDeserializer::deserialize(
            &m_ubjsonData->stream,
            &command.params,
            commandFormatVersion))
    {
        return nullptr;
    }

    return std::make_unique<UbjsonSerializedTransaction<CommandDescriptor>>(
        std::move(command),
        std::move(m_ubjsonData->serializedTransaction),
        commandFormatVersion);
}

} // namespace nx::clusterdb::engine
