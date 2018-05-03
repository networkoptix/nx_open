#include "last_connection.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace mobile {
namespace settings {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LastConnectionData, (json), LastConnectionData_Fields)

nx::utils::Url LastConnectionData::urlWithCredentials() const
{
    if (!url.isValid())
        return nx::utils::Url();

    auto result = url;
    result.setUserName(credentials.user);
    result.setPassword(credentials.password.value());
    return result;
}

} // namespace settings
} // namespace mobile
} // namespace client
} // namespace nx
