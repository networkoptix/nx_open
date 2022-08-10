// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

#include "map.h"

namespace nx::vms::api {

struct NX_VMS_API SystemSettingDescription
{
    QString label;

    /**%apidoc:enum
     * %value Null
     * %value Bool
     * %value Double
     * %value String
     * %value Array
     * %value Object
     */
    QJsonValue::Type type;

    bool isReadOnly = false;
    bool isWriteOnly = false;
    bool isOwnerOnly = false;
};
#define SystemSettingDescription_Fields (label)(type)(isReadOnly)(isWriteOnly)(isOwnerOnly)
QN_FUSION_DECLARE_FUNCTIONS(SystemSettingDescription, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(SystemSettingDescription, SystemSettingDescription_Fields)

struct SystemSettingDescriptions: std::map<QString, SystemSettingDescription>
{
    const SystemSettingDescription& front() const { return begin()->second; }
    QString getId() const { return (size() == 1) ? begin()->first : QString(); }
};

using SystemSettingsValues = Map<QString /*key*/, QJsonValue /*value*/>;

} // namespace nx::vms::api
