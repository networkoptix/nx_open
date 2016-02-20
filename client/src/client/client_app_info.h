#pragma once

struct QnClientAppInfo
{
    /** History name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();
    
    /** Real application name, visible to the user. */
    static QString applicationDisplayName();    
    
    /** Resource file id for videowall shortcut. */
    static int videoWallIconId();
};