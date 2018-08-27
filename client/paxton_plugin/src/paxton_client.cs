namespace nx {

public class PaxtonClient
{
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

    private readonly media_server_api.Connection m_connection;
}

} // namespace nx
