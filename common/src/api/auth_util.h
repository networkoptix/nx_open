#pragma once

#include <nx/network/http/auth_tools.h>

nx::String buildAuthKey(
    const nx::String& url,
    const nx_http::Credentials& credentials,
    const nx::String& nonce);
