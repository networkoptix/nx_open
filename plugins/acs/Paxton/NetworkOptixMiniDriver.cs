using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Reflection;
using System.Windows.Forms;
using Paxton.Net2.OemDvrInterfaces;
using log4net;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Drawing;
using Newtonsoft.Json;
using NxInterface;

namespace NetworkOptix.NxWitness.OemDvrMiniDriver {
    namespace Api {
        class Camera {
            public string id { get; set; }
            public string name { get; set; }
            public string model { get; set; }
        }

        class CurrentTime {
            public string value { get; set; }
        }

        class ConnectInfo {
            public string brand { get; set; }
            public string systemName { get; set; }
            public string version { get; set; }
            public int nxClusterProtoVersion { get; set; }
        }
    }

    public class NetworkOptixMiniDriver : IOemDvrMiniDriver {
        private static readonly ILog _logger = LogManager.GetLogger(MethodBase.GetCurrentMethod().DeclaringType);

        private static IPlugin axHDWitness;
        private static Control axHDWitnessForm;

//        private static AppDomain domain;

        private Panel m_surface;
        private OemDvrCamera[] _cameras;

        private long _tick;

        #region Implementation of IDvrMiniDriver

        public string GetSupplierIdentity() {
            const string supplierIdentity = @"NxWitness";

            _logger.InfoFormat("Queried supplier Oem DVR: {0}", supplierIdentity);
            return supplierIdentity;
        }

        public OemDvrStatus VerifyDvrCredentials(OemDvrConnection connectionInfo) {
            try {
                Uri url = new Uri(connectionInfo.HostName);
                int port = (int)connectionInfo.Port;
                if (port == 0)
                    port = url.Port;

                WebRequest request = HttpWebRequest.Create(String.Format("http://{0}:{1}/ec2/testConnection", url.Host, port));

                request.Credentials = new NetworkCredential(connectionInfo.UserId, connectionInfo.Password);

                WebResponse response = request.GetResponse();
                response.Close();

                return OemDvrStatus.Succeeded;
            }
            catch (Exception excp) {
                _logger.Error(excp);
            }

            return OemDvrStatus.InsufficientPriviledges;
        }

        public OemDvrStatus GetListOfCameras(OemDvrConnection connectionInfo, List<OemDvrCamera> cameras) {
            WebResponse response = null;

            try {
                Uri url = new Uri(connectionInfo.HostName);
                int port = (int)connectionInfo.Port;
                if (port == 0)
                    port = url.Port;

                WebRequest request = HttpWebRequest.Create(String.Format("http://{0}:{1}/ec2/getCamerasEx?format=json", url.Host, port));
                request.Credentials = new NetworkCredential(connectionInfo.UserId, connectionInfo.Password);
                response = request.GetResponse();

                StreamReader reader = new StreamReader(response.GetResponseStream());
                List<Api.Camera> apiCameras = JsonConvert.DeserializeObject<List<Api.Camera>>(reader.ReadToEnd());

                foreach (Api.Camera apiCamera in apiCameras) {
                    if (apiCamera.model.Contains("virtual desktop camera"))
                        continue;

                    cameras.Add(new OemDvrCamera(apiCamera.id, apiCamera.name));
                }

                _logger.InfoFormat("Found {0} cameras.", cameras.Count);
                return OemDvrStatus.Succeeded;
            }
            catch (Exception excp) {
                _logger.Error(excp);
            }
            finally {
                if (response != null)
                    response.Close();
            }

            _logger.Error("Could not list cameras.");
            return OemDvrStatus.FailedToListCameras;
        }

        private T LoadData<T>(string url, string login, string password) {
            WebRequest request = HttpWebRequest.Create(url);
            request.Credentials = new NetworkCredential(login, password);
            WebResponse response = request.GetResponse();
            StreamReader reader = new StreamReader(response.GetResponseStream());
            return JsonConvert.DeserializeObject<T>(reader.ReadToEnd());
        }

        public OemDvrStatus PlayFootage(OemDvrConnection cnInfo, OemDvrFootageRequest pbInfo, Panel pbSurface) {
            // MessageBox.Show("pe2");

            try {
                _tick = (long)((pbInfo.StartTimeUtc.ToUniversalTime() - new DateTime(1970, 1, 1)).TotalMilliseconds);
                Uri hostUrl = new Uri(cnInfo.HostName);
                Uri url = new Uri(String.Format("http://{0}:{1}@{2}:{3}", cnInfo.UserId, cnInfo.Password, hostUrl.Host, hostUrl.Port));

                if ((pbInfo.DvrCameras == null) || (pbInfo.DvrCameras.Length <= 0)) {
                    _logger.Error("Playback request without cameras.");
                    return OemDvrStatus.CameraListMissing;
                }

                if (pbInfo.DvrCameras.Length > 16) {
                    _logger.ErrorFormat("Playback requesting {0} cameras.", pbInfo.DvrCameras.Length);
                    return OemDvrStatus.NumberOfCamerasUnsupported;
                }

                switch (pbInfo.PlaybackFunction) {
                    case OemDvrPlaybackFunction.Start: {
                        // Calculate time offset
                        double timeOffset = 0;

                        DateTime startTime = DateTime.Now.ToUniversalTime();
                        Api.CurrentTime currentTime = LoadData<Api.CurrentTime>(url.ToString() + "ec2/getCurrentTime", cnInfo.UserId, cnInfo.Password);
                        DateTime endTime = DateTime.Now.ToUniversalTime();

                        DateTime localTime = startTime.AddMilliseconds((endTime - startTime).TotalMilliseconds / 2);
                        DateTime serverTime = new DateTime(1970, 1, 1, 0, 0, 0, 0).AddMilliseconds(Convert.ToInt64(currentTime.value));
                        timeOffset = (serverTime - localTime).TotalMilliseconds;

                        // Add timeoffset to tick
                        _tick += (long)timeOffset;

                        Api.ConnectInfo connectInfo = LoadData<Api.ConnectInfo>(url.ToString() + "ec2/connect", cnInfo.UserId, cnInfo.Password);

                        Version serverVersion = new Version(connectInfo.version);
                        int ourProtoVersion = CustomProperties.protoVersion;

                        if (CustomProperties.brand != connectInfo.brand) {
                            MessageBox.Show("Incompatible software", "Error", MessageBoxButtons.OK);
                            return OemDvrStatus.FootagePlaybackFailed;
                        }

                        if (ourProtoVersion < connectInfo.nxClusterProtoVersion) {
                            MessageBox.Show("Incompatible version. Please update... blah-blah-blah", "Error", MessageBoxButtons.OK);
                            return OemDvrStatus.FootagePlaybackFailed;
                        }

                        String assemblyName = String.Format("{0}Control.{1}", CustomProperties.className, connectInfo.nxClusterProtoVersion);
                        try {
                            axHDWitness = LoadAssembly(assemblyName);
                            axHDWitnessForm = (Control)axHDWitness;
                        }
                        catch (Exception e) {
                            string message = String.Format("You need to install the package version {0}.{1}.{2} to connect to this server.",
                                serverVersion.Major, serverVersion.Minor, serverVersion.Build);
                            MessageBox.Show(message, "Error", MessageBoxButtons.OK);
                            return OemDvrStatus.FootagePlaybackFailed;
                        }

                        Load(pbSurface, url);

                        _cameras = pbInfo.DvrCameras;
                        axHDWitness.play();
                        break;
                    }
                    case OemDvrPlaybackFunction.Forward:
                    case OemDvrPlaybackFunction.Backward:
                    case OemDvrPlaybackFunction.Pause: {
                            break;
                        }

                    case OemDvrPlaybackFunction.Stop: {
                            //                        AppDomain.Unload(domain);
                            break;
                        }

                    default: {
                            _logger.WarnFormat("Unsupported playback request: {0}", pbInfo.PlaybackFunction);
                            break;
                        }
                }

                return OemDvrStatus.Succeeded;
            }
            catch (WebException e) {
                _logger.Error(e);
                MessageBox.Show("Can't connect to the server. Verify server information in Camera Integration settings.");
            }
            catch (Exception e) {
                MessageBox.Show("Generic error occured. Please try again later.");
                _logger.Error(e);
            }

            return OemDvrStatus.FootagePlaybackFailed;
        }

        /// <summary>
        /// Control an existing video currently playing back. It must have been originally requested successfuly using PlayFootage.
        /// </summary>
        /// <param name="pbFunction">The play back function required.</param>
        /// <param name="speed">The speed as an index of 100 where motion is involved. 100 is normal speed.</param>
        /// <returns>The completion status of the request.</returns>
        public OemDvrStatus ControlFootage(OemDvrPlaybackFunction pbFunction, uint speed) {
            try {
                bool controlOk = false;
                switch (pbFunction) {
                    case OemDvrPlaybackFunction.Pause: {
                            axHDWitness.pause();
                            controlOk = true;
                            break;
                        }

                    case OemDvrPlaybackFunction.Forward: {
                            _logger.DebugFormat("Setting speed {0}", speed);
                            axHDWitness.setSpeed(1 + speed / 1000.0);
                            controlOk = true;
                            break;
                        }

                    case OemDvrPlaybackFunction.Backward: {
                            _logger.DebugFormat("Setting speed -{0}", speed);
                            axHDWitness.setSpeed(-1 - speed / 1000.0);
                            controlOk = true;
                            break;
                        }

                    // Close down the session, such that the next actions can be closing the application or starting new session.
                    case OemDvrPlaybackFunction.Stop: {
                            axHDWitness.pause();
                            controlOk = true;
                            break;
                        }

                    default: {
                            _logger.WarnFormat("Playback control {0} unsupported.", pbFunction);
                            break;
                        }
                }
                if (controlOk) {
                    return OemDvrStatus.Succeeded;
                }
            }
            catch (Exception excp) {
                _logger.Error(excp);
            }

            return OemDvrStatus.ControlRequestFailed;
        }
        #endregion

        void axAxHDWitness1_connectedProcessed(object sender, ConnectionProcessedEvent e) {
            updateControlSize();

            if (e.p_status != 0)
                return;

            var cameraIds = new List<string>();
            foreach (OemDvrCamera camera in _cameras) {
                cameraIds.Add(camera.CameraId);
            }

            string timestamp = _tick.ToString();
            axHDWitness.addResourcesToLayout(String.Join("|", cameraIds), timestamp);
        }

        private void PbSurfaceResize(object sender, EventArgs e) {
            updateControlSize();
        }

        private void updateControlSize() {
            short vidWidth = Convert.ToInt16(m_surface.Width);
            short vidHeight = Convert.ToInt16(m_surface.Height);
            axHDWitnessForm.SetBounds(0, 0, vidWidth, vidHeight);
        }

        private IPlugin LoadAssembly(string assemblyName) {
            string assemblyPath = Path.GetFullPath(assemblyName + ".dll");

            Assembly ptrAssembly = Assembly.Load(assemblyName);

            Type ti = typeof(IPlugin);
            foreach (Type item in ptrAssembly.GetTypes()) {
                if (!item.IsClass) continue;

                if (Array.Exists(item.GetInterfaces(), t => ti.IsAssignableFrom(t) )) {
                    return (IPlugin)Activator.CreateInstance(item);
                }
            }

            throw new Exception("Invalid DLL, Interface not found!");
        }

        private void Load(Panel pbSurface, Uri url) {
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

        /*
                static void Main(string[] args) {
                    OemDvrConnection connectionInfo = new OemDvrConnection("http://mono", 7001, "admin", "123");
                    List<OemDvrCamera> cameras = new List<OemDvrCamera>();

                    new NetworkOptixMiniDriver().GetListOfCameras(connectionInfo, cameras);

                    // Display the number of command line arguments:
                    System.Console.WriteLine(args.Length);
                } */
    }
}
