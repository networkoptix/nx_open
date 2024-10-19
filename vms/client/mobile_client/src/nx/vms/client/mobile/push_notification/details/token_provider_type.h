// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>

namespace nx::vms::client::mobile::details {

/**
 * Represents token provider type.
 * We support native APN for iOS. On Android we use Baidu in China and Firebase
 * for the rest of the world.
 */
NX_REFLECTION_ENUM_CLASS(TokenProviderType,
    undefined = -1,

    /**
     * Native Android provider. Previously (until 21.1) we supported only Firebase provider.
     * To smooth migration process value of 'firebase' provider item should be equal to '0'.
     */
    firebase,

    /** Native iOS provider. */
    apn,

    /** Native iOS provider with developer settings on the cloud. Used for development purposes. */
    apn_sandbox,

    /** Native China provider for Android. */
    baidu
)

} // namespace nx::vms::client::mobile::details
