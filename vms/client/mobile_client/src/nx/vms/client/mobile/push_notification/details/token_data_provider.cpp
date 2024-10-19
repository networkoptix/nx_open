// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "token_data_provider.h"

#include <QtCore/QtGlobal>

namespace nx::vms::client::mobile::details {

TokenDataProvider::Pointer TokenDataProvider::create()
{
    return Pointer(new TokenDataProvider());
}

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

TokenProviderType TokenDataProvider::type() const
{
    return TokenProviderType::undefined;
}

bool TokenDataProvider::requestTokenDataUpdate()
{
    // Empty implementation for desktop platform.
    return false;
}

bool TokenDataProvider::requestTokenDataReset()
{
    // Empty implementation for desktop platform.
    return false;
}

#endif

} // namespace nx::vms::client::mobile::details
