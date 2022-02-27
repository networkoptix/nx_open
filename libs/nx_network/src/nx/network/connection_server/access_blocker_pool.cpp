// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_blocker_pool.h"

namespace nx::network::server {

std::string toString(AuthResult authResult)
{
    switch (authResult)
    {
        case AuthResult::failure:
            return "failure";
        case AuthResult::success:
            return "success";
    }

    return "unknown";
}

} // namespace nx::network::server
