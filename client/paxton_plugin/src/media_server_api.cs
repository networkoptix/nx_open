using Newtonsoft.Json;
using System;
using System.IO;
using System.Net;
using System.Net.Http;
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

internal class CameraDataEx
{
    public string id;
    public string name;
}

// Wraps up a connection to the mediaserver.
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

    private T fromJson<T>(string data)
    {
        var reader = new JsonTextReader(new StringReader(data));
        return m_serializer.Deserialize<T>(reader);
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
        return fromJson<ModuleInformation>(responseData);
    }

    public async Task<CameraDataEx[]> getCamerasExAsync()
    {
        var responseData = await sendGetRequestAsync("ec2/getCamerasEx")
            .ConfigureAwait(false);
        return fromJson<CameraDataEx[]>(responseData);
    }

    private readonly string m_host;
    private readonly uint m_port;
    private readonly HttpClient m_client;
    private readonly JsonSerializer m_serializer = new JsonSerializer();
}

} // namespace nx.media_server_api
