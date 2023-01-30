// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manifest.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::rules {

QN_FUSION_ADAPT_STRUCT(ResourcePermission, nx_vms_rules_ResourcePermission_Fields)
void serialize(QnJsonContext* ctx, const ResourcePermission& value, QJsonValue* target)
{
    QnFusion::serialize(ctx, value, target);
}

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

} // namespace nx::vms::rules
