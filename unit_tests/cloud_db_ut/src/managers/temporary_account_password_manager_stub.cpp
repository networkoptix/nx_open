#include "temporary_account_password_manager_stub.h"

namespace nx {
namespace cdb {
namespace test {

boost::optional<TemporaryAccountCredentialsEx>
    TemporaryAccountPasswordManagerStub::getCredentialsByLogin(
        const std::string& /*login*/) const
{
    return boost::none;
}

} // namespace test
} // namespace cdb
} // namespace nx
