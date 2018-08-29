using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using System.Windows.Forms;
using log4net;
using nx.desktop_client_api;

namespace nx {

public class PaxtonClient
{
    private static readonly ILog m_logger =
        LogManager.GetLogger(MethodBase.GetCurrentMethod().DeclaringType);

    private static readonly uint kDefaultPort = 7001;

    public class Camera
    {
        public string id;
        public string name;
    }

    public enum PlaybackFunction
    {
        undefined,
        start,
        forward,
        backward,
        stop,
        pause,
    }

    public PaxtonClient()
    {

    }

    public PaxtonClient(string hostname, uint port, string user, string password)
    {
        m_host = hostname;
        m_port = port == 0 ? kDefaultPort : port;
        m_user = user;
        m_password = password;
        m_connection = new media_server_api.Connection(
            m_host,
            m_port,
            user,
            password);
    }

    public bool testConnection()
	{
        var moduleInformation = m_connection.getModuleInformationAsync().GetAwaiter().GetResult();
        m_logger.InfoFormat("Server found: {0}", moduleInformation.reply.customization);
        return moduleInformation.reply.customization == AppInfo.customization;
	}

    public IEnumerable<Camera> requestCameras()
    {
        var cameras = m_connection.getCamerasExAsync().GetAwaiter().GetResult();
        if (cameras == null)
            yield break;

        foreach (var dataEx in cameras)
            yield return new Camera() {id = dataEx.id, name = dataEx.name};
    }

    public bool playback(
        IEnumerable<string> cameraIds,
        DateTime startTimeUtc,
        uint speed)
    {
        if (m_connection == null)
        {
            m_logger.Error("Playback called without connection parameters.");
            return false;
        }

        if (m_process != null)
        {
            m_process.CloseMainWindow();
            m_process.Close();
        }

        m_process = Launcher.startClient(m_host, m_port, m_user, m_password, cameraIds, startTimeUtc);

        return m_process != null;
    }

    public bool control(PlaybackFunction function, uint speed)
    {

        return true;
    }

    private readonly media_server_api.Connection m_connection;
    private readonly string m_host;
    private readonly uint m_port;
    private readonly string m_user;
    private readonly string m_password;
    private Process m_process;

}

} // namespace nx
