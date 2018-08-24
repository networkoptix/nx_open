using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

namespace nx {

public class DriverImplementation
{
	public static bool testConnection(string hostname, uint port, string user, string password)
	{
        var connection = new nx.media_server_api.Connection(
            hostname,
            port,
            user,
            password);

        var moduleInformation = connection.getModuleInformation().GetAwaiter().GetResult();
        return moduleInformation.reply.customization == AppInfo.customization;
	}
}

} // namespace nx
