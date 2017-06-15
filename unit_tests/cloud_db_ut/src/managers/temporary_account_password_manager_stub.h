#pragma once

#include <nx/cloud/cdb/managers/temporary_account_password_manager.h>

namespace nx {
namespace cdb {
namespace test {

class TemporaryAccountPasswordManagerStub:
    public AbstractTemporaryAccountPasswordManager
{
public:
    virtual boost::optional<TemporaryAccountCredentialsEx> getCredentialsByLogin(
        const std::string& login) const override;
};

} // namespace test
} // namespace cdb
} // namespace nx
