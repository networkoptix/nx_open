#pragma once

#include <nx_ec/ec_proto_version.h>

#include "serializable_transaction.h"

namespace nx {
namespace cdb {
namespace ec2 {

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
        const TransactionTransportHeader& transportHeader,
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
template<typename TransactionDataType>
class UbjsonSerializedTransaction:
    public BaseUbjsonSerializedTransaction<SerializableTransaction<TransactionDataType>>
{
    typedef BaseUbjsonSerializedTransaction<SerializableTransaction<TransactionDataType>> BaseType;

public:
    UbjsonSerializedTransaction(
        ::ec2::QnTransaction<TransactionDataType> transaction,
        QByteArray ubjsonData,
        int serializedTransactionVersion)
        :
        BaseType(
            std::move(ubjsonData),
            serializedTransactionVersion,
            std::move(transaction))
    {
    }

    UbjsonSerializedTransaction(::ec2::QnTransaction<TransactionDataType> transaction):
        BaseType(
            QnUbjson::serialized(transaction),
            nx_ec::EC2_PROTO_VERSION,
            transaction)
    {
    }
};

class DummySerializable:
    public TransactionSerializer
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
        const TransactionTransportHeader& /*transportHeader*/,
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

} // namespace ec2
} // namespace cdb
} // namespace nx
