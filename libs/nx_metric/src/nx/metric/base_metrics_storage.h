// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>

namespace nx::metric {

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

        virtual QJsonValue toJson([[maybe_unused]] bool brief) const override
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

} // namespace nx::metric
