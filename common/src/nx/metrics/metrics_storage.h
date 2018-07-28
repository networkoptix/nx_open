#pragma once

#include <map>

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

namespace nx {
namespace metrics {

struct ParamSet
{
public:
    ParamSet() = default;
    ParamSet(const ParamSet&) = delete;
    ParamSet& operator=(const ParamSet&) = delete;

public:
    QJsonValue toJson(bool addDescription = true) const
    {
        QJsonObject result;
        for (auto& param: m_params)
        {
            if (addDescription)
            {
                QJsonObject paramJson;
                paramJson.insert("value", param.second->toJson(addDescription));
                paramJson.insert("description", param.second->description());
                result.insert(param.first, paramJson);
            }
            else
            {
                result.insert(param.first, param.second->toJson(addDescription));
            }
        }
        return result;
    }

protected:
    struct BaseParam
    {

        BaseParam(ParamSet* container, const QString& name, const QString& description):
            m_name(name),
            m_description(description)
        {
            container->add(name, this);
        }

        virtual ~BaseParam() = default;
        virtual QJsonValue toJson(bool addDescription) const = 0;
        const QString& name() { return m_name; }
        const QString& description() { return m_description; }

        BaseParam(const BaseParam&) = delete;
        BaseParam& operator=(const BaseParam&) = delete;

    protected:
        friend struct ParamSet;

        QString m_name;
        QString m_description;
    };

    template<typename T>
    struct Param final: BaseParam
    {
        const T& operator()() const { return m_value; }
        T& operator()() { return m_value; }

        virtual QJsonValue toJson(bool addDescription) const override
        {
            return toJsonInternal(addDescription);
        };

        template <typename Q=T>
        typename std::enable_if<!std::is_base_of<ParamSet, Q>::value, QJsonValue>::type
        toJsonInternal(bool /*addDescription*/) const
        {
            return QJsonValue(m_value);
        }

        template <typename Q=T>
        typename std::enable_if<std::is_base_of<ParamSet, Q>::value, QJsonValue>::type
        toJsonInternal(bool addDescription) const
        {
            return m_value.toJson(addDescription);
        }

    private:
        T  m_value;
    };

    void add(const QString& name, BaseParam* option)
    {
        NX_ASSERT(m_params.find(name) == m_params.end(), lit("Duplicate param: %1.").arg(name));
        m_params.emplace(name, option);
    }

private:
    std::map<QString, BaseParam*> m_params;
};

#define ADD_METRIC(type, name, description) Param<type> name{this, #name, description};

struct Storage: ParamSet
{
    ADD_METRIC(std::atomic_int, tcpConnections, "Amount of opened tcp connections");
    ADD_METRIC(std::atomic_int, transcoders, "Amount of video transcoding threads");
    ADD_METRIC(std::atomic_int, offlineStatus,
        "How many times resources have switched to the offline state");

    struct Transactions: ParamSet
    {
        ADD_METRIC(std::atomic_int, errors,
            "Amount of transactions that can't be written to DB due to SQL error");
        ADD_METRIC(std::atomic_int, success,
            "Total amount of transactions successfully written.");
        ADD_METRIC(std::atomic_int, local,
            "Total amount of local transactions written. Local transactions are written "
            "to the DB but not synchronized to another servers. 'Local' always <= 'success'");
    };
    ADD_METRIC(Transactions, transactions, "Database transactions statistics");
};

#undef ADD_METRIC

} // namespace metrics
} // namespace nx
