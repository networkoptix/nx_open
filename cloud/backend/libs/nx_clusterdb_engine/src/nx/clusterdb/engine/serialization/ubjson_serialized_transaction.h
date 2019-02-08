#pragma once

#include "serializable_transaction.h"

namespace nx::clusterdb::engine {

template<typename BaseType>
class BaseUbjsonSerializedTransaction:
    public BaseType
{
public:
    template<typename... ArgumentsForBase>
    BaseUbjsonSerializedTransaction(
        QByteArray ubjsonData,
        int serializedTransactionVersion,
        ArgumentsForBase... argumentsForBase)
        :
        BaseType(std::move(argumentsForBase)...),
        m_ubjsonData(std::move(ubjsonData)),
        m_serializedTransactionVersion(serializedTransactionVersion)
    {
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        int transactionFormatVersion) const override
    {
        if (targetFormat == Qn::UbjsonFormat /*&&
            transactionFormatVersion == m_serializedTransactionVersion*/)
        {
            return m_ubjsonData;
        }

        return BaseType::serialize(targetFormat, transactionFormatVersion);
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        const CommandTransportHeader& transportHeader,
        int transactionFormatVersion) const override
    {
        if (targetFormat == Qn::UbjsonFormat /*&&
            transactionFormatVersion == m_serializedTransactionVersion*/)
        {
            return QnUbjson::serialized(transportHeader.vmsTransportHeader) + m_ubjsonData;
        }

        return BaseType::serialize(targetFormat, transportHeader, transactionFormatVersion);
    }

private:
    QByteArray m_ubjsonData;
    const int m_serializedTransactionVersion;
};

/**
 * Encapsulates transaction and its ubjson presentation.
 */
template<typename CommandDescriptor>
class UbjsonSerializedTransaction:
    public BaseUbjsonSerializedTransaction<SerializableCommand<CommandDescriptor>>
{
    using base_type =
        BaseUbjsonSerializedTransaction<SerializableCommand<CommandDescriptor>>;

public:
    UbjsonSerializedTransaction(
        Command<typename CommandDescriptor::Data> transaction,
        QByteArray ubjsonData,
        int serializedTransactionVersion)
        :
        base_type(
            std::move(ubjsonData),
            serializedTransactionVersion,
            std::move(transaction))
    {
    }

    UbjsonSerializedTransaction(
        Command<typename CommandDescriptor::Data> command,
        int serializedTransactionVersion)
        :
        base_type(
            QnUbjson::serialized(command),
            serializedTransactionVersion,
            command)
    {
    }
};

class DummySerializable:
    public CommandSerializer
{
public:
    virtual nx::Buffer serialize(
        Qn::SerializationFormat /*targetFormat*/,
        int /*transactionFormatVersion*/) const override
    {
        NX_ASSERT(false);
        return nx::Buffer();
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat /*targetFormat*/,
        const CommandTransportHeader& /*transportHeader*/,
        int /*transactionFormatVersion*/) const override
    {
        NX_ASSERT(false);
        return nx::Buffer();
    }
};

/**
 * Ubjson presentation of some transaction without actual transaction structure.
 */
class UbjsonTransactionPresentation:
    public BaseUbjsonSerializedTransaction<DummySerializable>
{
    typedef BaseUbjsonSerializedTransaction<DummySerializable> BaseType;

public:
    UbjsonTransactionPresentation(
        QByteArray ubjsonData,
        int serializedTransactionVersion)
        :
        BaseType(std::move(ubjsonData), serializedTransactionVersion)
    {
    }
};

class TransactionUbjsonDataSource
{
public:
    QByteArray serializedTransaction;
    QnUbjsonReader<QByteArray> stream;

    TransactionUbjsonDataSource(QByteArray serializedTransactionVal):
        serializedTransaction(std::move(serializedTransactionVal)),
        stream(&serializedTransaction)
    {
    }

    TransactionUbjsonDataSource(const TransactionUbjsonDataSource&) = delete;
    TransactionUbjsonDataSource& operator=(const TransactionUbjsonDataSource&) = delete;
    TransactionUbjsonDataSource(TransactionUbjsonDataSource&&) = default;
    TransactionUbjsonDataSource& operator=(TransactionUbjsonDataSource&&) = default;
};

} // namespace nx::clusterdb::engine
