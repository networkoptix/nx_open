// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_promo_url.h"

#include <nx/branding.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

namespace {

const QString kSourceParameter = "utm_source";
const QString kMediumParameter = "utm_medium";
const QString kContentParameter = "utm_content";

const QString kSource = "notifications_saas_promo";

const QString kDesktopClientMedium = "client";
const QString kMobileClientMedium = "mobile";

QString medium(SaasPromoClientType clientType)
{
    switch (clientType)
    {
        case SaasPromoClientType::desktop:
            return kDesktopClientMedium;
        case SaasPromoClientType::mobile:
            return kMobileClientMedium;
    }

    NX_ASSERT(false, "Unexpected client type: %1", static_cast<int>(clientType));
    return {};
}

} // namespace

nx::Url saasPromoUrl(SaasPromoClientType clientType, const QString& siteId)
{
    const auto url = nx::Url::fromUserInput(nx::branding::saasPromoUrl());
    if (!url.isValid())
        return {};

    return nx::network::url::Builder(url)
        .addQueryItem(kSourceParameter, kSource)
        .addQueryItem(kMediumParameter, medium(clientType))
        .addQueryItem(kContentParameter, siteId);
}

} // namespace nx::vms::client::core
