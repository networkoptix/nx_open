// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit_details.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/rest/json.h>

#include "software_version_serialization.h"

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SessionDetails, (json), SessionDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PlaybackDetails, (json), PlaybackDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DeviceDetails, (json), DeviceDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ResourceDetails, (json), ResourceDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DescriptionDetails, (json), DescriptionDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MitmDetails, (json), MitmDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateDetails, (json), UpdateDetails_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ProxySessionDetails, (json), ProxySessionDetails_Fields)

void serialize_field(const AllAuditDetails::type& data, QVariant* target)
{
    QByteArray tmp;
    QJsonDetail::serialize_json(nx::network::rest::json::serialized(data), &tmp);
    serialize_field(tmp, target);
}

void deserialize_field(const QVariant& value, AllAuditDetails::type* target)
{
    QByteArray tmp;
    QnJsonContext jsonContext;
    jsonContext.setStrictMode(true);
    deserialize_field(value, &tmp);
    NX_ASSERT(QJson::deserialize(&jsonContext, tmp, target));
}

} // namespace nx::vms::api
