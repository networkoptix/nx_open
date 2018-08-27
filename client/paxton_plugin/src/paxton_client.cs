using System.Collections.Generic;

namespace nx {

public class PaxtonClient
{
    public class Camera
    {
        public string id;
        public string name;
    }

    public PaxtonClient(string hostname, uint port, string user, string password)
    {
        m_connection = new nx.media_server_api.Connection(
            hostname,
            port,
            user,
            password);
    }

    public bool testConnection()
	{
        var moduleInformation = m_connection.getModuleInformationAsync().GetAwaiter().GetResult();
        return moduleInformation.reply.customization == AppInfo.customization;
	}

    public IEnumerable<Camera> requestCameras()
    {
        var cameras = m_connection.getCamerasExAsync().GetAwaiter().GetResult();
        if (cameras != null)
        {
            foreach (var dataEx in cameras)
                yield return new Camera() {id = dataEx.id, name = dataEx.name};
        }
    }

    private readonly media_server_api.Connection m_connection;
}

} // namespace nx
