#pragma once

#include <rest/server/json_rest_result.h>
#include <core/resource_management/user_access_data.h>

struct QnPermissionsHelper
{
    /** Check if server is running in safe mode. */
    static bool isSafeMode();

    /** Check if user is system owner. */
    static bool hasOwnerPermissions(const Qn::UserAccessData& accessRights);

    /** Fill result with correct values and return fixed error code. */
    static int safeModeError(QnRestResult& result);

    /** Fill result with correct values and return fixed error code. */
    static int notOwnerError(QnRestResult& result);

    /** Fill plain result and contentType with correct values and return fixed error code. */
    static int safeModeError(QByteArray& result, QByteArray& contentType, Qn::SerializationFormat format = Qn::UnsupportedFormat, bool extraFormatting = false);

    /** Fill result with correct values and return fixed error code. */
    static int permissionsError(QnRestResult& result);

    /** Fill plain result and contentType with correct values and return fixed error code. */
    static int permissionsError(QByteArray& result, QByteArray& contentType, Qn::SerializationFormat format = Qn::UnsupportedFormat, bool extraFormatting = false);
};
