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
