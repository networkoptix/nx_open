using System;
using System.Diagnostics;
using System.IO;

namespace nx.desktop_client_api {

internal class Launcher
{
    private static string findLauncherBinary()
    {
        // Check latest applauncher first.
        var appData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var applauncherBinary = Path.Combine(
            appData,
            AppInfo.companyName,
            "applauncher",
            AppInfo.customization,
            "applauncher.exe");
        if (File.Exists(applauncherBinary))
            return applauncherBinary;

        foreach (var directory in new[]
            {
                Environment.GetEnvironmentVariable("ProgramW6432"),
                Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
                Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86)
            })
        {
            try
            {
                var clientDirectory = Path.Combine(
                    directory,
                    AppInfo.companyName,
                    AppInfo.productName,
                    "Client");

                foreach (var versionDirectory in Directory.EnumerateDirectories(clientDirectory))
                {
                    var minilauncherBinary = Path.Combine(
                        versionDirectory,
                        AppInfo.minilauncherBinaryName);
                    if (File.Exists(minilauncherBinary))
                        return minilauncherBinary;
                }
            }
            catch (Exception)
            {
                // Ignore different file search problems.
            }
        }

        return null;
    }

    public static Process startClient(string parameters)
    {
        var launcherBinary = findLauncherBinary();
        return launcherBinary != null ? Process.Start(launcherBinary, parameters) : null;
    }
}

}