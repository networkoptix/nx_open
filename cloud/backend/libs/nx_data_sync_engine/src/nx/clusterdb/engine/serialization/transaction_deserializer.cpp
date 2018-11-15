#include "transaction_deserializer.h"

#include <nx/fusion/model_functions.h>

namespace nx::clusterdb::engine {

bool TransactionDeserializer::deserialize(
    QnUbjsonReader<QByteArray>* const stream,
    CommandHeader* const transactionHeader,
    int /*transactionFormatVersion*/)
{
    return QnUbjson::deserialize(stream, transactionHeader);
}

std::unique_ptr<DeserializableCommandData> TransactionDeserializer::deserialize(
    Qn::SerializationFormat dataFormat,
    const std::string& originatorPeerId,
    int commandFormatVersion,
    QByteArray serializedCommand)
{
    if (dataFormat == Qn::SerializationFormat::UbjsonFormat)
    {
        return deserializeUbjson(
            QnUuid::fromStringSafe(originatorPeerId),
            commandFormatVersion,
            std::move(serializedCommand));
    }
    else if (dataFormat == Qn::SerializationFormat::JsonFormat)
    {
        return deserializeJson(
            QnUuid::fromStringSafe(originatorPeerId),
            std::move(serializedCommand));
    }
    else
    {
        return nullptr;
    }
}

std::unique_ptr<DeserializableCommandData>
    TransactionDeserializer::deserializeUbjson(
        const QnUuid& moduleGuid,
        int commandFormatVersion,
        QByteArray serializedCommand)
{
    CommandHeader commandHeader(moduleGuid);

    auto dataSource = std::make_unique<TransactionUbjsonDataSource>(
        std::move(serializedCommand));
    if (!deserialize(
            &dataSource->stream,
            &commandHeader,
            commandFormatVersion))
    {
        return nullptr;
    }

    return std::make_unique<DeserializableCommandData>(
        std::move(commandHeader),
        std::move(dataSource));
}

std::unique_ptr<DeserializableCommandData> TransactionDeserializer::deserializeJson(
    const QnUuid& moduleGuid,
    QByteArray serializedCommand)
{
    CommandHeader commandHeader(moduleGuid);
    QJsonObject tranObject;
    // TODO: #ak put tranObject to some cache for later use
    if (!QJson::deserialize(serializedCommand, &tranObject))
        return nullptr;
    
    if (!QJson::deserialize(tranObject["tran"], &commandHeader))
        return nullptr;

    return std::make_unique<DeserializableCommandData>(
        std::move(commandHeader),
        tranObject["tran"].toObject());
}

//-------------------------------------------------------------------------------------------------

DeserializableCommandData::DeserializableCommandData(
    CommandHeader commandHeader,
    QJsonObject jsonData)
    :
    m_commandHeader(std::move(commandHeader)),
    m_jsonData(std::move(jsonData))
{
}

DeserializableCommandData::DeserializableCommandData(
    CommandHeader commandHeader,
    std::unique_ptr<TransactionUbjsonDataSource> ubjsonData)
    :
    m_commandHeader(std::move(commandHeader)),
    m_ubjsonData(std::move(ubjsonData))
{
}
const CommandHeader& DeserializableCommandData::header() const
{
    return m_commandHeader;
}

} // namespace nx::clusterdb::engine
