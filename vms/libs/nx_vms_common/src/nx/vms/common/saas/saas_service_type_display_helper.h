// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

namespace nx::vms::common::saas {

/**
 * Provides human readable localized names for SaaS service types provided by the licensing server.
 */
class NX_VMS_COMMON_API ServiceTypeDisplayHelper
{
    Q_DECLARE_TR_FUNCTIONS(ServiceTypeDisplayStringHelper);

public:
    static QString serviceTypeDisplayString(const QString& serviceType);
};

} // nx::vms::common::saas
