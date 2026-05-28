// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/json/qjson.h>

namespace nx::vms::api::analytics {

struct FieldModel
{
    std::string type;
    std::string fieldName;
    std::string displayName;
    std::string description;
    QJsonObject properties;

    bool operator==(const FieldModel& other) const = default;
};
#define FieldModel_Fields (type)(fieldName)(displayName)(description)(properties)

QN_FUSION_DECLARE_FUNCTIONS(FieldModel, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(FieldModel, FieldModel_Fields)

struct IntegrationAction
{
    std::string id;
    std::string name;
    std::string description;
    bool isInstant;
    QList<FieldModel> fields;

    bool operator==(const IntegrationAction& other) const = default;
};
#define IntegrationAction_Fields (id)(name)(description)(isInstant)(fields)

QN_FUSION_DECLARE_FUNCTIONS(IntegrationAction, (json), NX_VMS_API)

NX_REFLECTION_INSTRUMENT(IntegrationAction, IntegrationAction_Fields);

} // namespace nx::vms::api::analytics
