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
using AxInterop.hdwitness;

namespace NetworkOptix.NxWitness.OemDvrMiniDriver {
    namespace Api {
        class Camera {
            public string id { get; set; }
            public string name { get; set; }
            public string model { get; set; }
        }
    }

    public class NetworkOptixMiniDriver : IOemDvrMiniDriver {
        private static readonly ILog _logger = LogManager.GetLogger(MethodBase.GetCurrentMethod().DeclaringType);
        private static AxAxHDWitness axAxHDWitness1;
        private Panel m_surface;
        private OemDvrCamera[] _cameras;

        private long _tick;

        #region Implementation of IDvrMiniDriver

        public string GetSupplierIdentity() {
            const string supplierIdentity = @"Network Optix DVR System";

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

        /// <summary>
        /// Request the playback of footage according to the required speed, direction and cameras using the supplied credentials on the OEM dll.
        ///     The image is to be rendered on the playback surface supplied.
        /// </summary>
        /// <param name="cnInfo">The user credentials structure.</param>
        /// <param name="pbInfo">The structure that holds the details of the footage and the cameras required.</param>
        /// <param name="pbSurface">A reference to the panel on to which the image is to be rendered.</param>
        /// <returns>The completion status of the request.</returns>
        public OemDvrStatus PlayFootage(OemDvrConnection cnInfo, OemDvrFootageRequest pbInfo, Panel pbSurface) {
            try {
                //DateTime dt = new DateTime( 2014 ,5, 21 , 19 ,30 , 30);
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

                Load(pbSurface, url);

                _cameras = pbInfo.DvrCameras;

                switch (pbInfo.PlaybackFunction) {
                    case OemDvrPlaybackFunction.Start: {
                            axAxHDWitness1.play();
                            break;
                        }
                    case OemDvrPlaybackFunction.Forward:
                    case OemDvrPlaybackFunction.Backward:
                    case OemDvrPlaybackFunction.Pause:
                    case OemDvrPlaybackFunction.Stop: {
                            break;
                        }

                    default: {
                            _logger.WarnFormat("Unsupported playback request: {0}", pbInfo.PlaybackFunction);
                            break;
                        }
                }

                return OemDvrStatus.Succeeded;
            }
            catch (Exception excp) {
                _logger.Error(excp);
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
                            axAxHDWitness1.pause();
                            controlOk = true;
                            break;
                        }

                    case OemDvrPlaybackFunction.Forward: {
                            _logger.DebugFormat("Setting speed {0}", speed);
                            axAxHDWitness1.setSpeed(1 + speed / 1000.0);
                            controlOk = true;
                            break;
                        }

                    case OemDvrPlaybackFunction.Backward: {
                            _logger.DebugFormat("Setting speed -{0}", speed);
                            axAxHDWitness1.setSpeed(-1 - speed / 1000.0);
                            controlOk = true;
                            break;
                        }

                    // Close down the session, such that the next actions can be closing the application or starting new session.
                    case OemDvrPlaybackFunction.Stop: {
                            axAxHDWitness1.pause();
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

        private void PbSurfaceResize(object sender, EventArgs e) {
            updateControlSize();
        }

        private void updateControlSize() {
            short vidWidth = Convert.ToInt16(m_surface.Width);
            short vidHeight = Convert.ToInt16(m_surface.Height);
            axAxHDWitness1.SetBounds(0, 0, vidWidth, vidHeight);
        }

        private void Load(Panel pbSurface, Uri url) {
            m_surface = pbSurface;
            pbSurface.SizeChanged += PbSurfaceResize;

            axAxHDWitness1 = new AxAxHDWitness();
            ((System.ComponentModel.ISupportInitialize)(axAxHDWitness1)).BeginInit();
            pbSurface.SuspendLayout();

            axAxHDWitness1.Enabled = true;
            axAxHDWitness1.Location = new System.Drawing.Point(10, 10);
            axAxHDWitness1.Name = "axAxHDWitness1";
            axAxHDWitness1.Size = new System.Drawing.Size(831, 509);
            axAxHDWitness1.TabIndex = 0;

            pbSurface.ClientSize = new System.Drawing.Size(1251, 829);
            pbSurface.Controls.Add(axAxHDWitness1);
            pbSurface.Name = "Form1";
            pbSurface.Text = "Form1";
            ((System.ComponentModel.ISupportInitialize)(axAxHDWitness1)).EndInit();
            pbSurface.ResumeLayout(false);

            axAxHDWitness1.connectionProcessed += axAxHDWitness1_connectedProcessed;

            axAxHDWitness1.reconnect(url.ToString());
        }

        void axAxHDWitness1_connectedProcessed(object sender, IAxHDWitnessEvents_connectionProcessedEvent e) {
            updateControlSize();

            if (e.p_status != 0)
                return;

            var cameraIds = new List<string>();
            foreach (OemDvrCamera camera in _cameras) {
                cameraIds.Add(camera.CameraId);
            }

            string timestamp = _tick.ToString();
            axAxHDWitness1.addResourcesToLayout(String.Join("|", cameraIds), timestamp);
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
