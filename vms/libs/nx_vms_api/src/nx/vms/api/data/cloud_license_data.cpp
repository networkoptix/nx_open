// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_license_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QDateTime CloudLicenseData::toDateTime(const QString& value)
{
    if (value.isEmpty())
        return QDateTime();

    QDateTime result = QDateTime::fromString(value, "yyyy-MM-dd hh:mm:ss");
    result.setTimeSpec(Qt::UTC);
    return result;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateCloudLicensesRequest, (json), UpdateCloudLicensesRequest_Fields)

} // namespace nx::vms::api
