// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manifest.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::rules {

QN_FUSION_ADAPT_STRUCT(PermissionsDescriptor, nx_vms_rules_PermissionsDescriptor_Fields)
void serialize(QnJsonContext* ctx, const PermissionsDescriptor& value, QJsonValue* target)
{
    QnFusion::serialize(ctx, value, target);
}

QN_FUSION_ADAPT_STRUCT(FieldDescriptor, nx_vms_rules_FieldDescriptor_Fields)
void serialize(QnJsonContext* ctx, const FieldDescriptor& value, QJsonValue* target)
{
    QnFusion::serialize(ctx, value, target);
}

QN_FUSION_ADAPT_STRUCT(ItemDescriptor, nx_vms_rules_ItemDescriptor_Fields)
void serialize(QnJsonContext* ctx, const ItemDescriptor& value, QJsonValue* target)
{
    QnFusion::serialize(ctx, value, target);
}

bool PermissionsDescriptor::empty() const
{
    return globalPermission == nx::vms::api::GlobalPermission::none
        && resourcePermissions.empty();
};

} // namespace nx::vms::rules
