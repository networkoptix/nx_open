using System;
using System.Collections.Generic;
using System.Reflection;
using System.Windows.Forms;
using log4net;

namespace nx {

public class PaxtonClient
{
    private static readonly ILog m_logger =
        LogManager.GetLogger(MethodBase.GetCurrentMethod().DeclaringType);

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
        m_connection = new media_server_api.Connection(
            hostname,
            port,
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

        return true;
    }

    public bool control(PlaybackFunction function, uint speed)
    {

        return true;
    }

    private readonly media_server_api.Connection m_connection;
}

} // namespace nx
