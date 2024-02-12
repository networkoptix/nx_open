// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "field.h"

namespace nx::vms::api::rules {

struct NX_VMS_API ActionBuilder
{
    Q_GADGET

public:
    nx::Uuid id;
    QString type;

    std::map<QString, Field> fields;
};

#define nx_vms_api_rules_ActionBuilder_Fields \
    (id)(type)(fields)

NX_VMS_API_DECLARE_STRUCT_EX(ActionBuilder, (json)(ubjson)(xml))

} // namespace nx::vms::api::rules
