// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "token_data_provider.h"

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

#include "token_data_watcher.h"

namespace nx::vms::client::mobile::details {

TokenProviderType TokenDataProvider::type() const
{
    return TokenProviderType::desktop;
}

bool TokenDataProvider::requestTokenDataUpdate()
{
    // Fake implementation for testing on desktop platform.
    TokenDataWatcher::instance().setData({type(), "DESKTOP_TOKEN_VALUE"});

    return true;
}

bool TokenDataProvider::requestTokenDataReset()
{
    // Fake implementation for testing on desktop platform.
    TokenDataWatcher::instance().resetData();

    return true;
}

} // namespace nx::vms::client::mobile::details

#endif
