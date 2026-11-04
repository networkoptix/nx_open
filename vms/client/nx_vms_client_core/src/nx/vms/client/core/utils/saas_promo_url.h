// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/utils/url.h>

namespace nx::vms::client::core {

enum class SaasPromoClientType
{
    desktop,
    mobile,
};

/**
 * SaaS promotion page URL with the client type and the Site ID added as tracking parameters, or
 * an invalid URL if the branding URL is invalid.
 */
NX_VMS_CLIENT_CORE_API nx::Url saasPromoUrl(SaasPromoClientType clientType, const QString& siteId);

} // namespace nx::vms::client::core
