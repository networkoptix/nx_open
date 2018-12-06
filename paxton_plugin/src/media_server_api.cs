using Newtonsoft.Json;
using System;
using System.IO;
using System.Net;
//using System.Net.Http;
using System.Threading.Tasks;

namespace nx.media_server_api {

internal class ModuleInformation
{
    public class Reply
    {
        public string customization;
        public int protoVersion;
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

        m_client = new WebClient {Credentials = credCache};

        // Ignore invalid SSL certificates.
        ServicePointManager.ServerCertificateValidationCallback =
            (senderX, certificate, chain, sslPolicyErrors) => true;

        // TLS 1.2. We must hardcode it as it is not supported directly in .NET 4.0
        ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072;
    }

    // Makes proper uri
    private Uri makeUri(string path, string query)
    {
        return new UriBuilder()
        {
            Port = (int)m_port,
            Scheme = Uri.UriSchemeHttps,
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

    private string sendGetRequest(string path, string query = "")
    {
        var uri = makeUri(path, query);
        var stream = m_client.OpenRead(uri);
        if (stream == null)
            return "";

        var reader = new StreamReader(stream);
        return reader.ReadToEnd();
    }

    public ModuleInformation getModuleInformation()
    {
        var responseData = sendGetRequest("api/moduleInformationAuthenticated");
        return fromJson<ModuleInformation>(responseData);
    }

    public CameraDataEx[] getCamerasEx()
    {
        var responseData = sendGetRequest("ec2/getCamerasEx");
        return fromJson<CameraDataEx[]>(responseData);
    }

    private readonly string m_host;
    private readonly uint m_port;
    private readonly WebClient m_client;
    private readonly JsonSerializer m_serializer = new JsonSerializer();
}

} // namespace nx.media_server_api
