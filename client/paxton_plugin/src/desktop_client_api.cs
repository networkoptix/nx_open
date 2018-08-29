using System;
using System.Diagnostics;
using System.IO;

namespace nx.desktop_client_api {

internal class Launcher
{
    public static Process startClient(string parameters)
    {
        var appData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var applauncherBinary = Path.Combine(
            appData,
            AppInfo.companyName,
            "applauncher",
            AppInfo.customization,
            "applauncher.exe");
        return Process.Start(applauncherBinary, parameters);
    }
}

}