using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Threading.Tasks;

namespace nx.media_server_api {

internal class ModuleInformation
{
    public class Reply
    {
        public string customization;
    }

    public Reply reply;
}

// Wraps up a connection to VMS server
internal class Connection
{
    public Connection(string host, uint port, string user, string password)
    {
        m_host = host;
        m_port = port;

        var credCache = new CredentialCache();
        var sampleUri = makeUri("", "");
        credCache.Add(sampleUri, "Digest", new NetworkCredential(user, password));

        m_client = new HttpClient( new HttpClientHandler { Credentials = credCache});
    }

    // Makes proper uri
    protected Uri makeUri(string path, string query)
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

    protected async Task<string> sendGetRequest(string path, string query = "")
    {
        var uri = makeUri(path, query);
        var response = await m_client.GetAsync(uri);
        return await response.Content.ReadAsStringAsync();
    }

    public async Task<ModuleInformation> getModuleInformation()
    {
        var responseData = await sendGetRequest("api/moduleInformationAuthenticated");
        var reader = new JsonTextReader(new StringReader(responseData));
        return m_serializer.Deserialize<ModuleInformation>(reader);
    }

    private string m_host;
    private uint m_port;
    private HttpClient m_client;
    private JsonSerializer m_serializer = new JsonSerializer();
}

} // namespace nx.media_server_api
