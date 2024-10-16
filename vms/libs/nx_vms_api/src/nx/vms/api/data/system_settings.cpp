// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_settings.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/rest/json.h>

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
    if (target->name != SiteSettingName::none)
    {
        switch (target->name)
        {
            #define VALUE(R, TARGET, ITEM) \
                case SiteSettingName::ITEM: \
                    if (deserialize(context, value, &TARGET->ITEM)) \
                        return true; \
                    context->setFailedKeyValue( \
                        {BOOST_PP_STRINGIZE(ITEM), QJson::serialized(value)}); \
                    return false;
            BOOST_PP_SEQ_FOR_EACH(VALUE, target, SaveableSiteSettings_Fields)
            #undef VALUE
        }
    }

    if (value.isObject() && value.toObject().contains("name"))
    {
        if (!deserialize(context, value, static_cast<SiteSettingsFilter*>(target)))
            return false;

        switch (target->name)
        {
            #define VALUE(R, CONTEXT, ITEM) \
                case SiteSettingName::ITEM: \
                    CONTEXT->setFailedKeyValue({"name", "\"" BOOST_PP_STRINGIZE(ITEM) "\""}); \
                    return false;
            BOOST_PP_SEQ_FOR_EACH(VALUE, context, UnsaveableSiteSettings_Fields)
            #undef VALUE
        }
    }

    return QnFusion::deserialize(context, value, target);
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
    return nx::network::rest::json::serialized(*this);
}

} // namespace nx::vms::api
