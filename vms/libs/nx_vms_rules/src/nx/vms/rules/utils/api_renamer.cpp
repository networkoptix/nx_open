// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "api_renamer.h"

#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/action_builder_fields/time_field.h>

namespace nx::vms::rules {
const IdRenamer kIdRenamer;

IdRenamer::IdRenamer()
{
    for (QString name: {"camera", "device", "server", "user", "layout", "eventType"})
    {
        m_toApi.insert(name + "Id", name);
        m_toApi.insert(name + "Ids", name + "s");
    }
}

QString IdRenamer::toApi(const QString& name) const
{
    return m_toApi.value(name, name);
}

struct NX_VMS_RULES_API TimeConverter: public UnitConverterBase
{
    virtual QString nameToApi(QString name) const override;
    virtual QJsonValue valueToApi(const QJsonValue& value) const override;
    virtual QJsonValue valueFromApi(const QJsonValue& value) const override;
};

QString TimeConverter::nameToApi(QString name) const
{
    return name + "S";
}

QJsonValue TimeConverter::valueToApi(const QJsonValue& value) const
{
    return value.toString().toLongLong() / std::micro::den;
}

QJsonValue TimeConverter::valueFromApi(const QJsonValue& value) const
{
    return QString::number(value.toInteger() * std::micro::den);
}

const UnitConverterBase* unitConverter(int type)
{
    static const UnitConverterBase base;
    static const TimeConverter time;

    // TODO: #amalov Consider using a registry if there more converters.
    if (type == QMetaType::fromType<std::chrono::microseconds>().id())
        return &time;

    return &base;
}

bool isTimeField(const QString& fieldId)
{
    return fieldId == fieldMetatype<TimeField>() || fieldId == fieldMetatype<OptionalTimeField>();
}

QString toApiFieldName(const QString& name, int fieldType)
{
    // Remove "Id", "Ids", if presented in complex fields.
    auto fieldName = kIdRenamer.toApi(name);

    // Format time value.
    return unitConverter(fieldType)->nameToApi(fieldName);
}

QString toApiFieldName(const QString& name, const QString& fieldId)
{
    const int type =
        isTimeField(fieldId) ? QMetaType::fromType<std::chrono::microseconds>().id() : -1;

    return toApiFieldName(name, type);
}

} // namespace nx::vms::rules
