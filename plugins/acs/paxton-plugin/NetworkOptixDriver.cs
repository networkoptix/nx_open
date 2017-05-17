using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Net;
using System.Reflection;

using Newtonsoft.Json;
using Newtonsoft.Json.Serialization;
using ErrorEventArgs = Newtonsoft.Json.Serialization.ErrorEventArgs;
using NxInterface;
using Paxton.Net2.OemDvrInterfaces;
using System.Windows.Forms;

namespace Paxton.NetworkOptixControl
{
    internal class UriHostConverter : JsonConverter
    {
        public override void WriteJson(JsonWriter writer, object value, JsonSerializer serializer)
        {
            throw new NotImplementedException();
        }

        public override object ReadJson(JsonReader reader, Type objectType, object existingValue, JsonSerializer serializer)
        {
            if (reader.TokenType == JsonToken.String)
            {
                String value = (String)serializer.Deserialize(reader, typeof(String));

                try
                {
                    Uri uri = (value.StartsWith("http://") || value.StartsWith("https://")) ? new Uri(value) : new Uri("http://" + value);
                    return uri.Host;
                }
                catch (InvalidOperationException)
                {
                }
                catch (UriFormatException)
                {
                }
                catch (ArgumentNullException)
                {
                }

                return value;
            }

            return null;
        }

        public override bool CanConvert(Type objectType)
        {
            return false;
        }
    }

    namespace Api
    {
        class Camera
        {
            public string id { get; set; }
            public string name { get; set; }
            public string model { get; set; }

            [JsonProperty("url")]
            [JsonConverter(typeof(UriHostConverter))]
            public string host { get; set; }
        }

        class CurrentTime
        {
            public string value { get; set; }
        }

        class ConnectInfo
        {
            public string brand { get; set; }
            public string systemName { get; set; }
            public string version { get; set; }
            public int nxClusterProtoVersion { get; set; }
            public string cloudHost { get; set; }
        }
    }

    public class NetworkOptixDriver : IOemDvrMiniDriver
    {
        private static readonly int PLAY_OFFSET_MS = 5000;
        private static readonly int DEFAULT_NX_PORT = 7001;

        private static IPlugin axHDWitness;
        private static Control axHDWitnessForm;

        private Panel m_surface;
        private OemDvrCamera[] _cameras;

        private static long _tick;

        private Api.ConnectInfo getConnectInfo(Uri url, string login, string password)
        {
            Api.ConnectInfo connectInfo = LoadData<Api.ConnectInfo>(url.ToString() + "ec2/connect", login, password);

            Version serverVersion = new Version(connectInfo.version);
            int ourProtoVersion = CustomProperties.protoVersion;

            if (CustomProperties.brand != connectInfo.brand)
            {
                MessageBox.Show("Incompatible software", "Error", MessageBoxButtons.OK);
                return null;
            }

            if (CustomProperties.cloudHost != connectInfo.cloudHost)
            {
                MessageBox.Show(
                    String.Format(
                        "Incompatible version. Please install plugin of version {0} to connect to this server. Error code 1.",
                    serverVersion.ToString(4)), "Error", MessageBoxButtons.OK);
                return null;
            }

            if (ourProtoVersion != connectInfo.nxClusterProtoVersion)
            {
                MessageBox.Show(
                    String.Format(
                        "Incompatible version. Please install plugin of version {0} to connect to this server. Error code 2.",
                    serverVersion.ToString(4)), "Error", MessageBoxButtons.OK);
                return null;
            }

            return connectInfo;
        }

        private Uri buildUrl(OemDvrConnection cnInfo)
        {
            string host = cnInfo.HostName.Contains(":") ? cnInfo.HostName : String.Format("{0}:{1}", cnInfo.HostName, DEFAULT_NX_PORT);
            Uri url = host.StartsWith("http://") ? new Uri(host) : new Uri("http://" + host);

            int port = (int)cnInfo.Port;
            if (port == 0)
                port = url.Port;

            return new UriBuilder("http", url.Host, port).Uri;
        }

        #region Implementation of IDvrMiniDriver

        public string GetSupplierIdentity()
        {
            const string supplierIdentity = CustomProperties.displayProductName;
            return supplierIdentity;
        }

        public OemDvrStatus VerifyDvrCredentials(OemDvrConnection cnInfo)
        {
            try
            {
                Uri url = buildUrl(cnInfo);
                Api.ConnectInfo connectInfo = getConnectInfo(url, cnInfo.UserId, cnInfo.Password);
                if (connectInfo != null)
                    return OemDvrStatus.Succeeded;
            }
            catch (Exception)
            {
            }

            MessageBox.Show("Can't connect to the server");

            return OemDvrStatus.InsufficientPriviledges;
        }

        public OemDvrStatus GetListOfCameras(OemDvrConnection cnInfo, List<OemDvrCamera> cameras)
        {
            WebResponse response = null;

            try
            {
                Uri baseUrl = buildUrl(cnInfo);

                WebRequest request = HttpWebRequest.Create(new Uri(baseUrl, "/ec2/getCamerasEx?format=json"));
                request.Credentials = new NetworkCredential(cnInfo.UserId, cnInfo.Password);
                response = request.GetResponse();

                StreamReader reader = new StreamReader(response.GetResponseStream());
                List<Api.Camera> apiCameras = JsonConvert.DeserializeObject<List<Api.Camera>>(reader.ReadToEnd(),
                    new JsonSerializerSettings
                    {
                        Error = delegate (object sender, ErrorEventArgs args)
                        {
                            args.ErrorContext.Handled = true;
                        }
                    });
                foreach (Api.Camera apiCamera in apiCameras)
                {
                    if (apiCamera.model.Contains("virtual desktop camera"))
                        continue;

                    cameras.Add(new OemDvrCamera(apiCamera.id, String.Format("{0} ({1})", apiCamera.name, apiCamera.host != null ? apiCamera.host : "")));
                }

                return OemDvrStatus.Succeeded;
            }
            catch (Exception)
            {
                return OemDvrStatus.FailedToListCameras;
            }
            finally
            {
                if (response != null)
                    response.Close();
            }
        }

        private T LoadData<T>(string url, string login, string password)
        {
            WebRequest request = HttpWebRequest.Create(url);
            request.Credentials = new NetworkCredential(login, password);
            WebResponse response = request.GetResponse();
            StreamReader reader = new StreamReader(response.GetResponseStream());
            return JsonConvert.DeserializeObject<T>(reader.ReadToEnd());
        }

        private long calculateTimeOffsetMs(Uri url, string login, string password)
        {
            DateTime startTime = DateTime.Now.ToUniversalTime();
            Api.CurrentTime currentTime = LoadData<Api.CurrentTime>(url.ToString() + "ec2/getCurrentTime", login, password);
            DateTime endTime = DateTime.Now.ToUniversalTime();

            DateTime localTime = startTime.AddMilliseconds((endTime - startTime).TotalMilliseconds / 2);
            DateTime serverTime = new DateTime(1970, 1, 1, 0, 0, 0, 0).AddMilliseconds(Convert.ToInt64(currentTime.value));

            long timeOffset = (long)(serverTime - localTime).TotalMilliseconds;
            timeOffset -= PLAY_OFFSET_MS;

            return timeOffset;
        }

        private long dateToMsecsSinceEpoch(DateTime dateTime)
        {
            return (long)((dateTime.ToUniversalTime() - new DateTime(1970, 1, 1)).TotalMilliseconds);
        }

        public OemDvrStatus PlayFootage(OemDvrConnection cnInfo, OemDvrFootageRequest pbInfo, Panel pbSurface)
        {
            try
            {
                _tick = dateToMsecsSinceEpoch(pbInfo.StartTimeUtc);

                Uri baseUrl = buildUrl(cnInfo);

                if ((pbInfo.DvrCameras == null) || (pbInfo.DvrCameras.Length <= 0))
                    return OemDvrStatus.CameraListMissing;

                if (pbInfo.DvrCameras.Length > 16)
                    return OemDvrStatus.NumberOfCamerasUnsupported;

                switch (pbInfo.PlaybackFunction)
                {
                    case OemDvrPlaybackFunction.Start:
                        {
                            Api.ConnectInfo connectInfo = getConnectInfo(baseUrl, cnInfo.UserId, cnInfo.Password);
                            if (connectInfo == null)
                                return OemDvrStatus.FootagePlaybackFailed;

                            Version serverVersion = new Version(connectInfo.version);

                            // Calculate time offset
                            long timeOffset = calculateTimeOffsetMs(baseUrl, cnInfo.UserId, cnInfo.Password);

                            // Add timeoffset to tick
                            _tick += timeOffset;

                            // Last minute tricks
                            // if time is within last 30 secs show live
                            // if it is less than 70 secs set time to current - 70 secs
                            long currentTime = dateToMsecsSinceEpoch(DateTime.Now);
                            long timeDiff = currentTime - _tick;
                            if (timeDiff < 30000)
                                _tick = -1;
                            else if (timeDiff < 70000)
                                _tick = currentTime - 70000;

                            String assemblyName = String.Format("{0}Control.{1}", CustomProperties.className, connectInfo.nxClusterProtoVersion);
                            try
                            {
                                axHDWitness = LoadAssembly(assemblyName);
                                axHDWitnessForm = (Control)axHDWitness;
                            }
                            catch (Exception)
                            {
                                string message = String.Format("You need to install the package version {0}.{1}.{2} to connect to this server.",
                                    serverVersion.Major, serverVersion.Minor, serverVersion.Build);
                                MessageBox.Show(message, "Error", MessageBoxButtons.OK);
                                return OemDvrStatus.FootagePlaybackFailed;
                            }

                            UriBuilder uriBuilder = new UriBuilder(baseUrl);
                            uriBuilder.UserName = cnInfo.UserId;
                            uriBuilder.Password = cnInfo.Password;
                            Load(pbSurface, uriBuilder.Uri);

                            _cameras = pbInfo.DvrCameras;
                            axHDWitness.play();
                            break;
                        }
                    case OemDvrPlaybackFunction.Forward:
                    case OemDvrPlaybackFunction.Backward:
                    case OemDvrPlaybackFunction.Pause:
                    case OemDvrPlaybackFunction.Stop:
                        break;

                    default:
                        break;
                }

                return OemDvrStatus.Succeeded;
            }
            catch (WebException)
            {
                MessageBox.Show("Can't connect to the server. Verify server information in Camera Integration settings.");
            }
            catch (Exception e)
            {
                MessageBox.Show(
                    String.Format(
                        "Generic error occured. Please try again later.\n\n{0}",
                        e.ToString()));
            }

            return OemDvrStatus.FootagePlaybackFailed;
        }

        /// <summary>
        /// Control an existing video currently playing back. It must have been originally requested successfuly using PlayFootage.
        /// </summary>
        /// <param name="pbFunction">The play back function required.</param>
        /// <param name="speed">The speed as an index of 100 where motion is involved. 100 is normal speed.</param>
        /// <returns>The completion status of the request.</returns>
        public OemDvrStatus ControlFootage(OemDvrPlaybackFunction pbFunction, uint speed)
        {
            try
            {
                bool controlOk = false;
                switch (pbFunction)
                {
                    case OemDvrPlaybackFunction.Pause:
                        {
                            axHDWitness.pause();
                            controlOk = true;
                            break;
                        }

                    case OemDvrPlaybackFunction.Forward:
                        {
                            axHDWitness.setSpeed(speed / 100);
                            controlOk = true;
                            break;
                        }

                    case OemDvrPlaybackFunction.Backward:
                        {
                            axHDWitness.setSpeed(-speed / 100);
                            controlOk = true;
                            break;
                        }

                    // Close down the session, such that the next actions can be closing the application or starting new session.
                    case OemDvrPlaybackFunction.Stop:
                        {
                            axHDWitness.pause();
                            controlOk = true;
                            break;
                        }

                    default:
                        {
                            break;
                        }
                }
                if (controlOk)
                {
                    return OemDvrStatus.Succeeded;
                }
            }
            catch (Exception)
            {
            }

            return OemDvrStatus.ControlRequestFailed;
        }
        #endregion

        void axAxHDWitness1_connectedProcessed(object sender, ConnectionProcessedEvent e)
        {
            updateControlSize();

            if (e.p_status != 0)
                return;

            var cameraIds = new List<string>();
            foreach (OemDvrCamera camera in _cameras)
            {
                cameraIds.Add(camera.CameraId);
            }

            string timestamp = _tick.ToString();
            axHDWitness.addResourcesToLayout(String.Join("|", cameraIds), timestamp);
        }

        private void PbSurfaceResize(object sender, EventArgs e)
        {
            updateControlSize();
        }

        private void updateControlSize()
        {
            short vidWidth = Convert.ToInt16(m_surface.Width);
            short vidHeight = Convert.ToInt16(m_surface.Height);
            axHDWitnessForm.SetBounds(0, 0, vidWidth, vidHeight);
        }

        private IPlugin LoadAssembly(string assemblyName)
        {
            string assemblyPath = Path.GetFullPath(assemblyName + ".dll");

            Assembly ptrAssembly = Assembly.Load(assemblyName);

            Type ti = typeof(IPlugin);
            foreach (Type item in ptrAssembly.GetTypes())
            {
                if (!item.IsClass) continue;

                if (Array.Exists(item.GetInterfaces(), t => ti.IsAssignableFrom(t)))
                {
                    return (IPlugin)Activator.CreateInstance(item);
                }
            }

            throw new Exception("Invalid DLL, Interface not found!");
        }

        private void Load(Panel pbSurface, Uri url)
        {
            m_surface = pbSurface;
            pbSurface.SizeChanged += PbSurfaceResize;

            ((System.ComponentModel.ISupportInitialize)(axHDWitness)).BeginInit();
            pbSurface.SuspendLayout();

            axHDWitnessForm.Enabled = true;
            axHDWitnessForm.Location = new System.Drawing.Point(10, 10);
            axHDWitnessForm.Name = "axHDWitness";
            axHDWitnessForm.Size = new System.Drawing.Size(831, 509);
            axHDWitnessForm.TabIndex = 0;

            pbSurface.ClientSize = new System.Drawing.Size(1251, 829);
            pbSurface.Controls.Add(axHDWitnessForm);
            pbSurface.Name = "Form1";
            pbSurface.Text = "Form1";
            ((System.ComponentModel.ISupportInitialize)(axHDWitness)).EndInit();
            pbSurface.ResumeLayout(false);

            axHDWitness.connectProcessed += axAxHDWitness1_connectedProcessed;

            axHDWitness.reconnect(url.ToString());
        }
    }

}
