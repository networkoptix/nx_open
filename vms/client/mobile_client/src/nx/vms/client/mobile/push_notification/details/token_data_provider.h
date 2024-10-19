// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include "token_data.h"

namespace nx::vms::client::mobile::details {

/** Specific token data provider implementation. */
class TokenDataProvider
{
public:
    using Pointer = std::shared_ptr<TokenDataProvider>;
    static Pointer create();

    /** Returns token provider type for current implementation. */
    TokenProviderType type() const;

    /**
     * Sends request to update token data with provider-specific implementation and updates
     * it in token data watcher.
     * @return "true" if request is implemented and went without errors, otherwise "false".
     */
    bool requestTokenDataUpdate();

    /**
     * Sends request to reset token data with provider-specific implementation and resets it
     * in token data watcher.
     * @return "true" if request is implemented and went without errors, otherwise "false".
     */
    bool requestTokenDataReset();

private:
    TokenDataProvider() = default;
};

} // namespace nx::vms::client::mobile::details
