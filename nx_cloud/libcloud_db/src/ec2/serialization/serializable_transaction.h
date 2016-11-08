#pragma once

#include <transaction/transaction.h>

#include "../transaction_serializer.h"

namespace nx {
namespace cdb {
namespace ec2 {

template<typename TransactionDataType>
class SerializableTransaction:
    public Serializable
{
public:
    SerializableTransaction(::ec2::QnTransaction<TransactionDataType> transaction):
        m_transaction(std::move(transaction))
    {
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        int transactionFormatVersion) const override
    {
        NX_ASSERT(transactionFormatVersion == nx_ec::EC2_PROTO_VERSION);

        switch (targetFormat)
        {
            case Qn::UbjsonFormat:
                return QnUbjson::serialized(m_transaction);

            default:
                NX_ASSERT(false);
                return nx::Buffer();
        }
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        const TransactionTransportHeader& transportHeader,
        int transactionFormatVersion) const override
    {
        NX_ASSERT(transactionFormatVersion == nx_ec::EC2_PROTO_VERSION);

        switch (targetFormat)
        {
            case Qn::UbjsonFormat:
                return QnUbjson::serialized(transportHeader.vmsTransportHeader) +
                    QnUbjson::serialized(m_transaction);

            default:
                NX_ASSERT(false);
                return nx::Buffer();
        }
    }

    const ::ec2::QnTransaction<TransactionDataType>& get() const
    {
        return m_transaction;
    }

    ::ec2::QnTransaction<TransactionDataType> take()
    {
        return std::move(m_transaction);
    }

private:
    ::ec2::QnTransaction<TransactionDataType> m_transaction;
};

template<typename TransactionDataType>
class UbjsonSerializedTransaction:
    public SerializableTransaction<TransactionDataType>
{
    typedef SerializableTransaction<TransactionDataType> BaseType;

public:
    UbjsonSerializedTransaction(
        ::ec2::QnTransaction<TransactionDataType> transaction,
        QByteArray ubjsonData,
        int serializedTransactionVersion)
        :
        BaseType(std::move(transaction)),
        m_ubjsonData(std::move(ubjsonData)),
        m_serializedTransactionVersion(serializedTransactionVersion)
    {
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        int transactionFormatVersion) const override
    {
        if (targetFormat == Qn::UbjsonFormat && 
            transactionFormatVersion == m_serializedTransactionVersion)
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
        if (targetFormat == Qn::UbjsonFormat &&
            transactionFormatVersion == m_serializedTransactionVersion)
        {
            return QnUbjson::serialized(transportHeader.vmsTransportHeader) + m_ubjsonData;
        }

        return BaseType::serialize(targetFormat, transportHeader, transactionFormatVersion);
    }

private:
    QByteArray m_ubjsonData;
    const int m_serializedTransactionVersion;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
