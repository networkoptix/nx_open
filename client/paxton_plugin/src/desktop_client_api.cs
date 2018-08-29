using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;

namespace nx.desktop_client_api {

internal class Launcher
{
    /**
     * Launch parameters command line should look like this:
     * --acs --no-fullscreen nx-vms://cloud-test.hdw.mx/client/localhost:7001/view?resources=ed93120e-0f50-3cdf-39c8-dd52a640688c&timestamp=1534513785000&auth=YWRtaW46cXdlYXNkMTIz
     */
    public static Process startClient(
        string host,
        uint port,
        string user,
        string password,
        IEnumerable<string> cameraIds,
        DateTime startTimeUtc)
    {
        var camerasString = String.Join(":", cameraIds);
        var timestamp = new DateTimeOffset(startTimeUtc).ToUnixTimeMilliseconds();
        var auth = Convert.ToBase64String(Encoding.UTF8.GetBytes($"{user}:{password}"));
        var url = $"{AppInfo.uriProtocol}://{AppInfo.cloudHost}/client/{host}:{port}" +
                  $"/view?resources={camerasString}&timestamp={timestamp}&auth={auth}";

        var parameters = String.Join(" ",
            new List<String>
            {
                "--acs",
                "--no-fullscreen",
                url
            });

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