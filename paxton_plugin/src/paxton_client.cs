using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Text;
using log4net;
using Paxton.Net2.OemDvrInterfaces;

namespace nx {

public static class PaxtonClient
{
    private static readonly ILog m_logger =
        LogManager.GetLogger(MethodBase.GetCurrentMethod().DeclaringType);

    private static Process m_process;

    private static readonly uint kDefaultPort = 7001;

    private static Dictionary<string, int> m_protocolByUrl = new Dictionary<string, int>();

    private static uint port(this OemDvrConnection connectionInfo)
    {
        return connectionInfo.Port == 0 ? kDefaultPort : connectionInfo.Port;
    }

    private static string key(this OemDvrConnection connectionInfo)
    {
        return $"{connectionInfo.HostName}:{connectionInfo.port()}";
    }

    private static media_server_api.Connection createConnection(OemDvrConnection connectionInfo)
    {
        m_logger.InfoFormat("Connecting to: {0}:{1} as {2}",
            connectionInfo.HostName,
            connectionInfo.port(),
            connectionInfo.UserId);

        return new media_server_api.Connection(
            connectionInfo.HostName,
            connectionInfo.port(),
            connectionInfo.UserId,
            connectionInfo.Password);
    }

    private static long unixTimeMilliseconds(DateTime value)
    {
        // Starting from .Net 4.6 we can use `new DateTimeOffset(value).ToUnixTimeMilliseconds()`
        var epoch = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);
        return (long) (value.ToUniversalTime() - epoch).TotalMilliseconds;
    }

    public static OemDvrStatus testConnection(OemDvrConnection connectionInfo)
	{
	    m_logger.InfoFormat("Testing connection to: {0}:{1} as {2}",
	        connectionInfo.HostName,
            connectionInfo.Port,
	        connectionInfo.UserId);

        var moduleInformation = createConnection(connectionInfo).getModuleInformation();

        m_logger.InfoFormat("Server found: {0}, protocol version: {1}",
            moduleInformation.reply.customization,
            moduleInformation.reply.protoVersion);
	    m_protocolByUrl[connectionInfo.key()] = moduleInformation.reply.protoVersion;

        return moduleInformation.reply.customization == AppInfo.customization
            ? OemDvrStatus.Succeeded
            : OemDvrStatus.UnknownHost;
	}

    public static IEnumerable<OemDvrCamera> requestCameras(OemDvrConnection connectionInfo)
    {
        var cameras = createConnection(connectionInfo).getCamerasEx();

        if (cameras == null)
            yield break;

        foreach (var dataEx in cameras)
            yield return new OemDvrCamera(dataEx.id, dataEx.name);
    }

    public static OemDvrStatus playback(
        OemDvrConnection connectionInfo,
        OemDvrFootageRequest footageRequest)
    {
        if (m_process != null)
        {
            m_process.CloseMainWindow();
            m_process.Close();
        }

        var protocolKey = connectionInfo.key();
        if (!m_protocolByUrl.ContainsKey(protocolKey))
        {
            m_logger.Info("Protocol was not found in cache, requesting info");
            testConnection(connectionInfo);
        }

        var protocolVersion = m_protocolByUrl.ContainsKey(protocolKey)
            ? m_protocolByUrl[protocolKey]
            : 0;

        if (protocolVersion == 0)
            m_logger.Warn("Info request failed!");
        else
            m_logger.InfoFormat("Protocol received: {0}", protocolVersion);

        /**
         * Command line parameters should look like this:
         * --acs
         * --no-fullscreen
         * --proto=3042
         * nx-vms://cloud-test.hdw.mx/client/localhost:7001/view
         *      ?resources=ed93120e-0f50-3cdf-39c8-dd52a640688c
         *      &timestamp=1534513785000
         *      &auth=YWRtaW46cXdlYXNkMTIz
         * Please note: auth should be the last to avoid parsing issues.
         */
        var camerasString = string.Join(":", footageRequest.DvrCameras.Select(x => x.CameraId));

        // Paxton combines cameras to a single camera with ids joined via '|'. Don't know why.
        camerasString = camerasString.Replace('|', ':');

        var timestamp = unixTimeMilliseconds(footageRequest.StartTimeUtc);

        var auth = Convert.ToBase64String(
            Encoding.UTF8.GetBytes($"{connectionInfo.UserId}:{connectionInfo.Password}"));
        var url = $"{AppInfo.uriProtocol}://" +
                  $"{AppInfo.cloudHost}/" +
                  "client/" +
                  $"{connectionInfo.HostName}:{connectionInfo.port()}/" +
                  $"view?resources={camerasString}&timestamp={timestamp}&auth={auth}";

        var parameters = string.Join(" ",
            new List<string>
            {
                "--acs",
                "--no-fullscreen",
                protocolVersion == 0 ? "" : $"--proto={protocolVersion}",
                url
            });
        m_logger.DebugFormat("Run applauncher with parameters: {0}", parameters);

        m_process = desktop_client_api.Launcher.startClient(parameters);
        Environment.Exit(0);
        return m_process != null ? OemDvrStatus.Succeeded : OemDvrStatus.FootagePlaybackFailed;
    }

    public static OemDvrStatus control(OemDvrPlaybackFunction function, uint speed)
    {
        return m_process != null ? OemDvrStatus.Succeeded : OemDvrStatus.ControlRequestFailed;
    }

}

} // namespace nx
