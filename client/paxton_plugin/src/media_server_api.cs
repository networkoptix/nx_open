using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace nx.media_server_api {

internal class ModuleInformation
{
    public class Reply
    {
        public string customization;
    }

    public Reply reply;
}

// Wraps up a connection to the mediaserver.
internal class Connection
{
    public Connection(string host, uint port, string user, string password)
    {
        m_host = host;
        m_port = port == 0 ? 7001 : port;
        m_credentials = new NetworkCredential(user, password);

        var credCache = new CredentialCache();
        var sampleUri = makeUri("", "");
        credCache.Add(sampleUri, "Digest", m_credentials);

        m_client = new HttpClient( new HttpClientHandler { Credentials = credCache});
    }

    // Makes proper uri
    private Uri makeUri(string path, string query)
    {
        return new UriBuilder()
        {
            Port = (int)m_port,
            Scheme = Uri.UriSchemeHttp,
            Host = m_host,
            Path = path,
            Query = query,
        }.Uri;
    }

    private async Task<string> sendGetRequestAsync(string path, string query = "")
    {
        var uri = makeUri(path, query);
        return await m_client.GetStringAsync(uri).ConfigureAwait(false);
    }

    public async Task<ModuleInformation> getModuleInformationAsync()
    {
        var responseData = await sendGetRequestAsync("api/moduleInformationAuthenticated")
            .ConfigureAwait(false);
        MessageBox.Show("received response " + responseData);
        var reader = new JsonTextReader(new StringReader(responseData));
        return m_serializer.Deserialize<ModuleInformation>(reader);
    }

    private readonly string m_host;
    private readonly uint m_port;
    private readonly HttpClient m_client;
    private readonly NetworkCredential m_credentials;
    private readonly JsonSerializer m_serializer = new JsonSerializer();
}

} // namespace nx.media_server_api
