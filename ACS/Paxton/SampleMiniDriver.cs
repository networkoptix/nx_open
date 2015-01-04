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

namespace Company.Product.OemDvrMiniDriver {
    namespace Api {
        class Camera {
            public string physicalId { get; set; }
            public string name { get; set; }
        }
    }

    public class SampleMiniDriver : IOemDvrMiniDriver {
        private static readonly ILog _logger = LogManager.GetLogger(MethodBase.GetCurrentMethod().DeclaringType);
        // private axhdwitnessLib.AxHDWitness _axhdwitness;

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

                request.Credentials = new System.Net.NetworkCredential(connectionInfo.UserId, connectionInfo.Password);

                WebResponse response = request.GetResponse();
                response.Close();

                return OemDvrStatus.Succeeded;
            } catch (Exception excp) {
                _logger.Error(excp);
            }

            return OemDvrStatus.InsufficientPriviledges;
        }

        List<string> ParseString(string str) {
            List<string> elements = new List<string>();
            int beginPos = 0;
            int endPos = 0;
            for (int j = 0; j < str.Length; j++) {
                if ((str[j] == ',') && (j == 0 || str[j - 1] != '\\')) {
                    endPos = j;
                    if (beginPos < endPos) {
                        elements.Add(str.Substring(beginPos, endPos - beginPos));
                    } else elements.Add("");
                    beginPos = endPos + 1;
                }
            };
            if (beginPos != 0) {
                if (beginPos < str.Length) {
                    elements.Add(str.Substring(beginPos, str.Length - beginPos));
                } else elements.Add("");
            }
            return elements;
        }

        public OemDvrStatus GetListOfCameras(OemDvrConnection connectionInfo, List<OemDvrCamera> cameras) {
            WebResponse response = null;

            try {
                MessageBox.Show("Vagina");

                Uri url = new Uri(connectionInfo.HostName);
                int port = (int)connectionInfo.Port;
                if (port == 0)
                    port = url.Port;

                WebRequest request = HttpWebRequest.Create(String.Format("http://{0}:{1}/ec2/getCameras?format=json", url.Host, port));

                request.Credentials = new System.Net.NetworkCredential(connectionInfo.UserId, connectionInfo.Password);

                response = request.GetResponse();

                StreamReader reader = new StreamReader(response.GetResponseStream());
                cameras.Add(new OemDvrCamera("fake", "name"));
                List<Api.Camera> apiCameras = JsonConvert.DeserializeObject<List<Api.Camera>>(reader.ReadToEnd());

                foreach(Api.Camera apiCamera in apiCameras) {
                    cameras.Add(new OemDvrCamera(apiCamera.physicalId.ToUpper(), apiCamera.name));
                }
                
                _logger.InfoFormat("Found {0} cameras.", cameras.Count);
                return OemDvrStatus.Succeeded;
            } catch (Exception excp) {
                _logger.Error(excp);
            } finally {
                if (response != null)
                    response.Close();
            }

            _logger.Error("Could not list cameras.");
            return OemDvrStatus.FailedToListCameras;
        }
        /// <summary>
        /// Request the playback of footage according to the required speed, direction and cameras using the supplied credentials on the OEM dll.
        ///		The image is to be rendered on the playback surface supplied.
        /// </summary>
        /// <param name="cnInfo">The user credentials structure.</param>
        /// <param name="pbInfo">The structure that holds the details of the footage and the cameras required.</param>
        /// <param name="pbSurface">A reference to the panel on to which the image is to be rendered.</param>
        /// <returns>The completion status of the request.</returns>
        public OemDvrStatus PlayFootage(OemDvrConnection cnInfo, OemDvrFootageRequest pbInfo, Panel pbSurface) {
            try {

                MessageBox.Show("Penis");

                //DateTime dt = new DateTime( 2014 ,5, 21 , 19 ,30 , 30);
                double tick = (pbInfo.StartTimeUtc.ToUniversalTime() - new DateTime(1970, 1, 1)).TotalMilliseconds;



                Uri url = new Uri(cnInfo.HostName);



                if ((pbInfo.DvrCameras == null) || (pbInfo.DvrCameras.Length <= 0)) {
                    _logger.Error("Playback request without cameras.");
                    return OemDvrStatus.CameraListMissing;
                }
                if (pbInfo.DvrCameras.Length > 16) {
                    _logger.ErrorFormat("Playback requesting {0} cameras.", pbInfo.DvrCameras.Length);
                    return OemDvrStatus.NumberOfCamerasUnsupported;
                }

                Load();

                switch (pbInfo.PlaybackFunction) {
                    case OemDvrPlaybackFunction.Start: {
                            Play();
                            break;
                        }
                    case OemDvrPlaybackFunction.Forward: {
                            break;
                        }

                    case OemDvrPlaybackFunction.Backward: {
                            break;
                        }
                    case OemDvrPlaybackFunction.Pause: {
                            Pause();
                            break;
                        }
                    case OemDvrPlaybackFunction.Stop: {
                            Stop();
                            break;
                        }

                    default: {
                            _logger.WarnFormat("Unsupported playback request: {0}", pbInfo.PlaybackFunction);
                            break;
                        }
                }

                return OemDvrStatus.Succeeded;
            } catch (Exception excp) {
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
                            Pause();
                            controlOk = true;
                            break;
                        }

                    case OemDvrPlaybackFunction.Forward:
                    case OemDvrPlaybackFunction.Backward: {
                            Play();
                            controlOk = true;
                            break;
                        }

                    // Close down the session, such that the next actions can be closing the application or starting new session.
                    case OemDvrPlaybackFunction.Stop: {
                            Stop();
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
            } catch (Exception excp) {
                _logger.Error(excp);
            }

            return OemDvrStatus.ControlRequestFailed;
        }
        #endregion

        private void Load() {
        }

        private void Play() {
            int a = 1;
        }

        private void Stop() {
            int b = 2;
        }

        private void Pause() {
        }
/*
        static void Main(string[] args) {
            OemDvrConnection connectionInfo = new OemDvrConnection("http://mono", 7001, "admin", "123");
            List<OemDvrCamera> cameras = new List<OemDvrCamera>();

            new SampleMiniDriver().GetListOfCameras(connectionInfo, cameras);

            // Display the number of command line arguments:
            System.Console.WriteLine(args.Length);
        } */
    }
}
