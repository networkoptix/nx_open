#pragma once

#include <rest/server/json_rest_result.h>

struct QnPermissionsHelper
{
    /** Check if server is running in safe mode. */
    static bool isSafeMode();

    /** Fill result with correct values and return fixed error code. */
    static int safeModeError(QnRestResult& result);

    /** Fill plain result and contentType with correct values and return fixed error code. */
    static int safeModeError(QByteArray& result, QByteArray& contentType, Qn::SerializationFormat format = Qn::UnsupportedFormat, bool extraFormatting = false);

    /** Check if giver user has enough global access rights. */
    static bool hasGlobalPermission(const QnUuid& userId, Qn::GlobalPermission required);

    /** Fill result with correct values and return fixed error code. */
    static int permissionsError(QnRestResult& result);

    /** Fill plain result and contentType with correct values and return fixed error code. */
    static int permissionsError(QByteArray& result, QByteArray& contentType, Qn::SerializationFormat format = Qn::UnsupportedFormat, bool extraFormatting = false);
};
