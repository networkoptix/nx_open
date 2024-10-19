// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QMetaType>
#include <QtCore/QString>

#include <nx/reflect/instrument.h>

#include "token_provider_type.h"

namespace nx::vms::client::mobile::details {

struct LocalPushSettings;

using OptionalUserId = std::optional<QString>;

/**
 * Represents token data. Includes all information which is needed by our cloud to send push
 * notifications to the device.
 */
struct TokenData
{
    /** Mandatory field which represents token provider type. */
    TokenProviderType provider = TokenProviderType::undefined;

    /** Mandatory field which represents device token. */
    QString token;

    /** User identifier for Baidu provider. Not used with the other providers. */
    OptionalUserId userId;

    /** Makes empty non-valid token data with empty fields. */
    static TokenData makeEmpty();

    /** Makes token data for Firebase push notifications. */
    static TokenData makeFirebase(const QString& token);

    /** Makes token data for iOS push notifications. */
    static TokenData makeApn(const QString& token);

    /** Makes token data for Baidu push notifications. */
    static TokenData makeBaidu(
        const QString& token,
        const QString& userId);

    /** Makes fake token data for internal usage. */
    static TokenData makeInternal();

    /** Makes current token data empty. */
    void reset();

    /** Returns true if all mandatory fields are filled and provider is defined. */
    bool isValid() const;

    bool operator == (const TokenData& /*other*/) const = default;
};
NX_REFLECTION_INSTRUMENT(TokenData, (provider)(token)(userId))

} // namespace nx::vms::client::mobile::details

Q_DECLARE_METATYPE(nx::vms::client::mobile::details::TokenData)
