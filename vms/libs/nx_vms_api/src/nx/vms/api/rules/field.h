// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QMap>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::rules {

struct NX_VMS_API Field
{
    Q_GADGET

public:
    QString name;
    QString metatype;
    QMap<QString, QJsonValue> props;
};

#define nx_vms_api_rules_Field_Fields \
    (name)(metatype)(props)

QN_FUSION_DECLARE_FUNCTIONS(Field, (json)(ubjson)(xml), NX_VMS_API)

} // namespace nx::vms::api::rules
