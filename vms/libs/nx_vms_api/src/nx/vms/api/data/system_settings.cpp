// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_settings.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaveableSystemSettings, (json), SaveableSystemSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemSettings, (json), SystemSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemSettingsFilter, (json), SystemSettingsFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SiteSettingsFilter, (json), SiteSettingsFilter_Fields)
QN_FUSION_ADAPT_STRUCT(SaveableSiteSettings, SaveableSiteSettings_Fields)
QN_FUSION_ADAPT_STRUCT(SiteSettings, SiteSettings_Fields)

void serialize(QnJsonContext* context, const SaveableSiteSettings& value, QJsonValue* target)
{
    return QnFusion::serialize(context, value, target);
}

bool deserialize(QnJsonContext* context, const QJsonValue& value, SaveableSiteSettings* target)
{
    auto object = value.toObject();
    auto name = object.take("name");
    if (!deserialize(
        context, QJsonObject{{"name", name}}, static_cast<SiteSettingsFilter*>(target)))
    {
        return false;
    }

    if (target->name == SiteSettingName::none)
        return QnFusion::deserialize(context, value, target);

    auto nestedValue = object.take("_");
    if (nestedValue.isUndefined())
        nestedValue = object;
    switch (target->name)
    {
        #define VALUE(R, TARGET, ITEM) \
            case SiteSettingName::ITEM: \
                if (deserialize(context, nestedValue, &TARGET->ITEM)) \
                    return true; \
                context->setFailedKeyValue( \
                    {BOOST_PP_STRINGIZE(ITEM), QJson::serialized(nestedValue)}); \
                return false;
        BOOST_PP_SEQ_FOR_EACH(VALUE, target, SaveableSiteSettings_Fields)
        #undef VALUE

        #define VALUE(R, CONTEXT, ITEM) \
            case SiteSettingName::ITEM: \
                CONTEXT->setFailedKeyValue({"name", "\"" BOOST_PP_STRINGIZE(ITEM) "\""}); \
                return false;
        BOOST_PP_SEQ_FOR_EACH(VALUE, context, UnsaveableSiteSettings_Fields)
        #undef VALUE
    }

    context->setFailedKeyValue({name.toString(), QJson::serialized(nestedValue)});
    return false;
}

void serialize(QnJsonContext* context, const SiteSettings& value, QJsonValue* target)
{
    switch (value.name)
    {
        #define VALUE(R, VALUE, ITEM) \
            case SiteSettingName::ITEM: \
                return serialize(context, VALUE.ITEM, target);
        BOOST_PP_SEQ_FOR_EACH(VALUE, value, SiteSettings_Fields)
        #undef VALUE

        case SiteSettingName::none:
            return QnFusion::serialize(context, value, target);
    }
}

bool deserialize(QnJsonContext* context, const QJsonValue& value, SiteSettings* target)
{
    return QnFusion::deserialize(context, value, target);
}

QJsonValue SiteSettings::front() const
{
    QnJsonContext context;
    context.setSerializeMapToObject(true);
    context.setChronoSerializedAsDouble(true);
    QJsonValue result;
    QJson::serialize(&context, *this, &result);
    return result;
}

} // namespace nx::vms::api
