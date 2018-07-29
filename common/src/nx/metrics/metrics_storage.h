#pragma once

#include <map>
#include <QtCore/QJsonObject>

namespace nx {
namespace metrics {

struct ParamSet
{
    ParamSet() = default;
    ParamSet(const ParamSet&) = delete;
    ParamSet& operator=(const ParamSet&) = delete;

    QJsonValue toJson(bool addDescription = true) const
    {
        QJsonObject result;
        for (const auto& param: m_params)
        {
            const auto value = param.second->toJson(addDescription);
            if (addDescription)
                result.insert(param.first, QJsonObject{
                    {"value", value}, 
                    {"description", param.second->description()}});
            else
                result.insert(param.first, value);
        }
        return result;
    }
private:
    struct BaseParam
    {
        BaseParam(ParamSet* paramSet, const QString& name, const QString& description):
            m_name(name), m_description(description)
        {
            paramSet->add(name, this);
        }
        BaseParam(const BaseParam&) = delete;
        BaseParam& operator=(const BaseParam&) = delete;
        virtual ~BaseParam() = default;

        virtual QJsonValue toJson(bool addDescription) const = 0;
        const QString& name() const { return m_name; }
        const QString& description() const { return m_description; }
    private:
        const QString m_name;
        const QString m_description;
    };

    template<typename T>
    struct Param final: BaseParam
    {
        using BaseParam::BaseParam;

        const T& operator()() const { return m_value; }
        T& operator()() { return m_value; }

        virtual QJsonValue toJson(bool addDescription) const override
        {
            if constexpr(std::is_base_of<ParamSet, T>::value)
                return m_value.toJson(addDescription);
            else
                return QJsonValue(m_value);
        }
    private:
        T  m_value;
    };

    void add(const QString& name, BaseParam* value)
    {
        NX_ASSERT(m_params.count(name) == 0, lm("Duplicate parameter name: %1").arg(name));
        m_params.emplace(name, value);
    }

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
