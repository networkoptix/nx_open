#pragma once

#define QN_AX_CLASS_ID                  "${ax.classId}"
#define QN_AX_INTERFACE_ID              "${ax.interfaceId}"
#define QN_AX_EVENTS_ID                 "${ax.eventsId}"
#define QN_AX_TYPELIB_ID                "${ax.typeLibId}"
#define QN_AX_APP_ID                    "${ax.appId}"

struct QnAxClientAppInfo
{
    /** History name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();

    /** Real application name, visible to the user. */
    static QString applicationDisplayName();
};
