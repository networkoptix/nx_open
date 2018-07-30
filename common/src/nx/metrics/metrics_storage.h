#pragma once

#include <vector>
#include <QtCore/QJsonObject>

namespace nx {
namespace metrics {

struct ParamSet
{
    ParamSet() = default;
    ParamSet(const ParamSet&) = delete;
    ParamSet& operator=(const ParamSet&) = delete;

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
        BaseParam(ParamSet* paramSet, const QString& name, const QString& description):
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
            if constexpr(std::is_base_of<ParamSet, T>::value)
                return m_value.toJson(brief);
            else
                return QJsonValue(m_value);
        }
    private:
        T  m_value{};
    };

private:
    std::vector<BaseParam*> m_params;
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
