// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QMap>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "../data/data_macros.h"

namespace nx::vms::api::rules {

struct NX_VMS_API Field
{
    /**%apidoc String description of this event or action field type.
     * %example streamQuality
     */
    QString type;

    /**%apidoc[opt] Key value pair map of field property names to values. */
    QMap<QString, QJsonValue> props;
};

#define nx_vms_api_rules_Field_Fields (type)(props)
NX_VMS_API_DECLARE_STRUCT_EX(Field, (json)(ubjson))
NX_REFLECTION_INSTRUMENT(Field, nx_vms_api_rules_Field_Fields)

} // namespace nx::vms::api::rules
