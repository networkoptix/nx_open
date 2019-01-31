#pragma once

#include <vector>
#include <QtCore/QMap>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

namespace nx {
namespace metrics {

/*
 * Helper function to check whether type is a map or QMap
 */
template<class, class = std::void_t<>> struct isMap: std::false_type {};
template<class T> struct isMap<T, std::void_t<typename T::key_type>>: std::true_type {};

struct ParameterSet
{
    ParameterSet() = default;
    ParameterSet(const ParameterSet&) = delete;
    ParameterSet& operator=(const ParameterSet&) = delete;

    QJsonValue toJson(bool brief = false) const
    {
        QJsonObject result;
        for (const auto& param: m_params)
        {
            if (brief)
                result.insert(param->name(), param->toJson(brief));
            else
                result.insert(param->name(), QJsonObject{
                    {"value", param->toJson(brief)},
                    {"description", param->description()}});
        }
        return result;
    }

protected:
    struct BaseParam
    {
        BaseParam(ParameterSet* paramSet, const QString& name, const QString& description):
            m_name(name), m_description(description)
        {
            paramSet->m_params.push_back(this);
        }
        BaseParam(const BaseParam&) = delete;
        BaseParam& operator=(const BaseParam&) = delete;
        virtual ~BaseParam() = default;

        virtual QJsonValue toJson(bool brief) const = 0;
        const QString& name() const { return m_name; }
        const QString& description() const { return m_description; }
    private:
        const QString m_name;
        const QString m_description;
    };

    template <typename T>
    struct Param final: BaseParam
    {
        using BaseParam::BaseParam;

        const T& operator()() const { return m_value; }
        T& operator()() { return m_value; }

        virtual QJsonValue toJson(bool brief) const override
        {
            if constexpr(isMap<T>::value)
            {
                QVariantMap result;
                for (auto itr = m_value.begin(); itr != m_value.end(); ++itr)
                   result.insert(itr.key(), itr.value());
                return QJsonObject::fromVariantMap((result));
            }
            else if constexpr(std::is_base_of<ParameterSet, T>::value)
            {
                return m_value.toJson(brief);
            }
            else
            {
                return QJsonValue(m_value);
            }
        }
    private:
        T  m_value{};
    };

private:
    std::vector<BaseParam*> m_params;
};

#define NX_METRICS_ADD(type, name, description) Param<type> name{this, #name, description};

struct Storage: ParameterSet
{
    struct TcpConnections: ParameterSet
    {
        NX_METRICS_ADD(std::atomic_int, total,
            "Total amount of opened TCP connections with any type");
        NX_METRICS_ADD(std::atomic_int, rtsp,
            "Amount of opened RTSP connections");
        NX_METRICS_ADD(std::atomic_int, hls,
            "Amount of opened HLS connections");
        NX_METRICS_ADD(std::atomic_int, progressiveDownloading,
            "Amount of opened progressive downloading connections");
        NX_METRICS_ADD(std::atomic_int, p2p,
            "Amount of opened p2p connections");
        NX_METRICS_ADD(std::atomic<qint64>, totalBytesSent, "Total tcp bytes sent");
    };
    NX_METRICS_ADD(TcpConnections, tcpConnections, "Opened TCP connections");

    NX_METRICS_ADD(std::atomic_int, transcoders, "Amount of video transcoding threads");
    NX_METRICS_ADD(std::atomic_int, offlineStatus,
        "How many times resources have switched to the offline state");

    struct Transactions: ParameterSet
    {
        NX_METRICS_ADD(std::atomic_int, errors,
            "Amount of transactions that can't be written to DB due to SQL error");
        NX_METRICS_ADD(std::atomic_int, success,
            "Total amount of transactions successfully written.");
        NX_METRICS_ADD(std::atomic_int, local,
            "Total amount of local transactions written. Local transactions are written "
            "to the DB but not synchronized to another servers. 'Local' always <= 'success'");
        NX_METRICS_ADD(std::atomic<qint64>, logSize, "Total size of transaction log in bytes");
    };
    NX_METRICS_ADD(Transactions, transactions, "Database transactions statistics");

    struct P2pStatisticsData: ParameterSet
    {
        using DataSentByMessageType = QMap<QString, qint64>;
        NX_METRICS_ADD(DataSentByMessageType, dataSentByMessageType, "Amount of sent data in bytes by p2p message type");
    };
    NX_METRICS_ADD(P2pStatisticsData, p2pCounters, "P2p statistics");
};

#undef NX_METRICS_ADD

} // namespace metrics
} // namespace nx
