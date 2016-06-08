#pragma once

#include <QtCore/QString>

namespace nx
{
    namespace vms
    {
        namespace utils
        {
            struct NX_VMS_UTILS_API AppInfo
            {
                /** Native protocol for URI links. */
                static QString nativeUriProtocol();
            };
        }
    }
}