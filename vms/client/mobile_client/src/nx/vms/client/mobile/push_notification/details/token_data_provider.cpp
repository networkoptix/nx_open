// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "token_data_provider.h"

namespace nx::vms::client::mobile::details {

TokenDataProvider::Pointer TokenDataProvider::create()
{
    return Pointer(new TokenDataProvider());
}

} // namespace nx::vms::client::mobile::details
