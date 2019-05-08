#pragma once

#include <string>

#include <nx/fusion/model_functions.h>
#include <nx/utils/uuid.h>

#include <nx/vms/api/data/timestamp.h>

namespace nx::clusterdb::engine {

using TimestampType = nx::vms::api::Timestamp;

struct NX_DATA_SYNC_ENGINE_API HistoryAttributes
{
    /** Id of user or entity who created transaction. */
    QnUuid author;

    bool operator==(const HistoryAttributes& rhs) const;
};

#define HistoryAttributes_Fields (author)

QN_FUSION_DECLARE_FUNCTIONS(
    HistoryAttributes,
    (json)(ubjson),
    NX_DATA_SYNC_ENGINE_API)

//-------------------------------------------------------------------------------------------------

struct NX_DATA_SYNC_ENGINE_API PersistentInfo
{
    QnUuid dbID;
    qint32 sequence = 0;
    TimestampType timestamp;

    PersistentInfo();

    bool isNull() const;

    bool operator==(const PersistentInfo& other) const;
};

NX_DATA_SYNC_ENGINE_API uint qHash(const nx::clusterdb::engine::PersistentInfo& id);

#define PersistentInfo_Fields (dbID)(sequence)(timestamp)

QN_FUSION_DECLARE_FUNCTIONS(
    PersistentInfo,
    (json)(ubjson),
    NX_DATA_SYNC_ENGINE_API)

//-------------------------------------------------------------------------------------------------

class NX_DATA_SYNC_ENGINE_API CommandHeader
{
public:
    using PersistentInfo = engine::PersistentInfo;

    CommandHeader() = default;
    CommandHeader(int value, QnUuid peerId);

    int command = -1;
    /** Id of peer that generated transaction. */
    QnUuid peerID;
    PersistentInfo persistentInfo;

    /** Introduced for compatibility with ::ec2::QnAbstractTransaction. */
    int transactionType = 2;
    HistoryAttributes historyAttributes;

    bool operator==(const CommandHeader& rhs) const;
    bool operator!=(const CommandHeader& rhs) const;
};

#define CommandHeader_Fields (command)(peerID)(persistentInfo)(transactionType)(historyAttributes)

QN_FUSION_DECLARE_FUNCTIONS(
    CommandHeader,
    (json)(ubjson),
    NX_DATA_SYNC_ENGINE_API)

NX_DATA_SYNC_ENGINE_API std::string toString(const CommandHeader& header);

//-------------------------------------------------------------------------------------------------

template<typename Data>
class Command:
    public CommandHeader
{
    using base_type = CommandHeader;

public:
    Data params;

    Command() = default;

    explicit Command(CommandHeader header):
        base_type(std::move(header))
    {
    }

    Command(int commandCode, QnUuid peerId):
        base_type(commandCode, std::move(peerId))
    {
    }

    const CommandHeader& header() const
    {
        return *this;
    }

    void setHeader(CommandHeader commandHeader)
    {
        // TODO: #ak Not good, but I cannot change basic ec2 classes.
        static_cast<CommandHeader&>(*this) = std::move(commandHeader);
    }

    bool operator==(const Command& rhs) const
    {
        return header() == rhs.header()
            && params == rhs.params;
    }

    bool operator!=(const Command& rhs) const
    {
        return !(*this == rhs);
    }
};

template<class T>
void serialize(QnJsonContext* ctx, const Command<T>& tran, QJsonValue* target)
{
    QJson::serialize(ctx, static_cast<const CommandHeader&>(tran), target);
    QJsonObject localTarget = target->toObject();
    QJson::serialize(ctx, tran.params, QLatin1String("params"), &localTarget);
    *target = localTarget;
}

template<class T>
bool deserialize(QnJsonContext* ctx, const QJsonValue& value, Command<T>* target)
{
    return
        QJson::deserialize(ctx, value, static_cast<CommandHeader*>(target)) &&
        QJson::deserialize(ctx, value.toObject(), QLatin1String("params"), &target->params);
}

/*QnUbjson format functions for Command<T>*/
template <class T, class Output>
void serialize(const Command<T>& transaction, QnUbjsonWriter<Output>* stream)
{
    QnUbjson::serialize(static_cast<const CommandHeader&>(transaction), stream);
    QnUbjson::serialize(transaction.params, stream);
}

template <class T, class Input>
bool deserialize(QnUbjsonReader<Input>* stream, Command<T>* transaction)
{
    return
        QnUbjson::deserialize(stream, static_cast<CommandHeader*>(transaction)) &&
        QnUbjson::deserialize(stream, &transaction->params);
}

} // namespace nx::clusterdb::engine
