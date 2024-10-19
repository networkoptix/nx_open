// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "token_data.h"

#include "build_info.h"
#include "push_notification_structures.h"

namespace {

static const QString kEmptyToken;
static const nx::vms::client::mobile::details::OptionalUserId kNoUserId;

} // namespace

namespace nx::vms::client::mobile::details {

TokenData TokenData::makeEmpty()
{
    return TokenData{TokenProviderType::undefined, kEmptyToken, kNoUserId};
}

TokenData TokenData::makeFirebase(const QString& token)
{
    return TokenData{TokenProviderType::firebase, token, kNoUserId};
}

TokenData TokenData::makeApn(const QString& token)
{
    const auto type = BuildInfo::useProdNotificationSettings()
        ? TokenProviderType::apn
        : TokenProviderType::apn_sandbox;

    return TokenData{type, token, kNoUserId};
}

TokenData TokenData::makeBaidu(
    const QString& token,
    const QString& userId)
{
    return TokenData{TokenProviderType::baidu, token, userId};
}

TokenData TokenData::makeInternal()
{
    return TokenData{TokenProviderType::undefined, "SOME_FAKE_TOKEN_VALUE", {}};
}

void TokenData::reset()
{
    *this = TokenData::makeEmpty();
}

bool TokenData::isValid() const
{
    if (provider == TokenProviderType::undefined || token.isEmpty())
        return false;

    return provider != TokenProviderType::baidu || !userId.value_or(QString()).isEmpty();
}

} // namespace nx::vms::client::mobile::details
