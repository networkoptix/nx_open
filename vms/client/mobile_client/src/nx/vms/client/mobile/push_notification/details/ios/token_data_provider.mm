// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../token_data_provider.h"

#include <UIKit/UIApplication.h>

#include "../build_info.h"
#include "../token_data_watcher.h"

namespace nx::vms::client::mobile::details {

TokenProviderType TokenDataProvider::type() const
{
    return BuildInfo::useProdNotificationSettings()
        ? TokenProviderType::apn
        : TokenProviderType::apn_sandbox;
}

bool TokenDataProvider::requestTokenDataUpdate()
{
    const auto application = [UIApplication sharedApplication];
    [application registerForRemoteNotifications];

    // TokeDataWatcher::setData is called by the application delegate.

    return true;
}

bool TokenDataProvider::requestTokenDataReset()
{
    const auto application = [UIApplication sharedApplication];
    [application unregisterForRemoteNotifications];

    TokenDataWatcher::instance().resetData();
    return true;
}

} // namespace nx::vms::client::mobile::details
