// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API StorageEncryptionData
{
    /**%apidoc Password to encrypt storage. Encryption is turned off if password is empty. */
    QString password;

    /**%apidoc[opt] Make this password current if true. New archive will be recorded with
     * this password. Otherwise just put the password to the list to enable decrypting existing
     * archive. Default value is 'true'.
     */
    bool makeCurrent = true;

    /**%apidoc[opt]:base64 Use specific salt or auto generate it if absent. */
    QByteArray salt;
};
#define StorageEncryptionData_Fields (password)(makeCurrent)(salt)

NX_VMS_API_DECLARE_STRUCT_AND_LIST(StorageEncryptionData)


struct NX_VMS_API AesKeyData
{
    /**%apidoc
     * One of the keys to decrypt archive. The second key 'cipherKey' is hidden in API and can't be
     * read.
     */
    std::string ivVect;

    /**%apidoc Issue date. */
    std::chrono::microseconds issueDateUs{};

    /**%apidoc[opt] New archive will be recorded with this password if the value is true. */
    bool isCurrent = false;
};
#define AesKeyData_Fields (ivVect)(issueDateUs)(isCurrent)

NX_VMS_API_DECLARE_STRUCT_AND_LIST(AesKeyData)

} // namespace nx::vms::api
