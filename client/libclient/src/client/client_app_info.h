#pragma once

struct QnClientAppInfo
{
    /** History name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();

    /** Real application name, visible to the user. */
    static QString applicationDisplayName();

    /** Name of the default (up to 2.5) installed applauncher binary in the filesystem (applauncher, HD Witness Launcher.exe). */
    static QString minilauncherBinaryName();

    /** Name of the new applauncher binary in the filesystem (applaucher-bin, applauncher.exe). */
    static QString applauncherBinaryName();

    /** Name of the client binary in the filesystem (client-bin, HD Witness.exe). */
    static QString clientBinaryName();

    /** Resource file id for videowall shortcut. */
    static int videoWallIconId();
};
