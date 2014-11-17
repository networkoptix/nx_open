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



namespace MyLibVLC
{
    

    // http://www.videolan.org/developers/vlc/doc/doxygen/html/group__libvlc.html

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    struct libvlc_exception_t
    {
        public int b_raised;
        public int i_code;
        [MarshalAs(UnmanagedType.LPStr)]
        public string psz_message;
    }

    static class LibVlc
    {
        const string DLL_FOLDER = "NetworkOptixVLCVideoPlayer";

        #region core

        [DllImport(DLL_FOLDER+"\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr libvlc_new(int argc, [In] string[] str_array);

        //[DllImport("libvlc")]
        //public static extern IntPtr libvlc_new(Int argc, [MarshalAs(UnmanagedType.LPArray,
        //  ArraySubType = UnmanagedType.LPStr)] string[] argv);

        [DllImport(DLL_FOLDER + "\\libvlc.dll")]
        public static extern void libvlc_release(IntPtr instance);
        #endregion

        //LIBVLC_API libvlc_media_t * 	libvlc_media_new_location (libvlc_instance_t *p_instance, const char *psz_mrl)

        #region media
        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr libvlc_media_new_location(IntPtr p_instance, [In] string psz_mrl);

        //[DllImport("libvlc")]
        //public static extern IntPtr libvlc_media_new_location(IntPtr p_instance,[MarshalAs(UnmanagedType.LPStr)] string psz_mrl);

        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void libvlc_media_release(IntPtr p_meta_desc);
        #endregion

        #region media player
        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr libvlc_media_player_new_from_media(IntPtr media);

        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void libvlc_media_player_release(IntPtr player);

        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void libvlc_media_player_set_hwnd(IntPtr player, IntPtr drawable);

        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libvlc_media_player_play(IntPtr player);

        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void libvlc_media_player_pause(IntPtr player);

        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void libvlc_media_player_stop(IntPtr player);

        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void libvlc_media_player_set_time(IntPtr p_mi, Int64 i_time);

        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern Int64 libvlc_media_player_get_time(IntPtr p_mi);

        [DllImport(DLL_FOLDER + "\\libvlc.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libvlc_media_player_set_rate(IntPtr p_mi, float rate);


        #endregion
    }
}

namespace Company.Product.OemDvrMiniDriver
{
    struct Video
    {
        public IntPtr instance;
        public IntPtr player;
        public IntPtr media;
        public string cameraId;
        public Button window;
    }
	/// <summary>
	/// This is the main point of entry for your DVR system.
	/// </summary>
	public class SampleMiniDriver : IOemDvrMiniDriver
	{
		private static readonly ILog _logger = LogManager.GetLogger(MethodBase.GetCurrentMethod().DeclaringType);
        private static List<Video> m_video;
        private static Panel m_panel;
        private static float m_playerSpeed = 1.0f;
        private static string m_host;
        private static uint m_port = 0;
        private static long m_startTimeinMicroseconds = 0;
        private static Stopwatch m_timer = new Stopwatch();
        private static long m_timeAdvance = 0;


		#region Implementation of IDvrMiniDriver

		/// <summary>
		/// Return the string you would expect to see in the Net2 OEM supplier list, that identifies your system.
		/// </summary>
		/// <returns></returns>
		public string GetSupplierIdentity()
		{
			const string supplierIdentity = @"Network Optix DVR System";

			_logger.InfoFormat("Queried supplier Oem DVR: {0}", supplierIdentity);
			return supplierIdentity;
		}
        
		/// <summary>
		/// Verifies the host and credentials that will be stored in the Net2 database and with which requested footage will be viewed.
		/// </summary>
		/// <param name="connectionInfo">A structure identifying the host and user information.</param>
		/// <returns>OemDvrStatus value. Likely alternatives are : UnknownHost, InvalidUserIdPassword, InsufficientPriviledges</returns>
		public OemDvrStatus VerifyDvrCredentials(OemDvrConnection connectionInfo)
		{
			try
			{
                
                Uri url = new Uri(connectionInfo.HostName);
                int port = (int)connectionInfo.Port;
                if (port == 0)
                    port = url.Port;

                string url_str = String.Format("http://{0}:{1}/ec2/testConnection", url.Host, port);

                WebRequest request = HttpWebRequest.Create(url_str);

                request.Credentials = new System.Net.NetworkCredential(connectionInfo.UserId, connectionInfo.Password);

                WebResponse response = request.GetResponse();
                
                return OemDvrStatus.Succeeded;
			}
			catch (Exception excp)
			{
				_logger.Error(excp);
			}

			// The more precise the implementation, the better support can be provided.
			return OemDvrStatus.InsufficientPriviledges;
		}
        List<string> ParseString(string str)
        {
            List<string> elements = new List<string>();
            int beginPos = 0;
            int endPos = 0;
            for (int j = 0; j < str.Length; j++)
            {
                if ((str[j] == ',') && (j == 0 || str[j - 1] != '\\'))
                {
                    endPos = j;
                    if (beginPos < endPos)
                    {
                        elements.Add(str.Substring(beginPos, endPos - beginPos));
                    }
                    else elements.Add("");
                    beginPos = endPos + 1;
                }
            };
            if (beginPos != 0)
            {
                if (beginPos < str.Length)
                {
                    elements.Add(str.Substring(beginPos, str.Length - beginPos));
                }
                else elements.Add("");
            }
            return elements;
        }

		/// <summary>
		/// Obtains a list of available cameras. Expect the credentials used to be the same as for VerifyDvrCredentials.
		/// </summary>
		/// <param name="connectionInfo">A structure identifying the host and user information.</param>
		/// <param name="cameras">An updatable list structure into which the camera detail can be added.</param>
		/// <returns>OemDvrStatus value. See the enumeration for detail.</returns>
		public OemDvrStatus GetListOfCameras(OemDvrConnection connectionInfo, List<OemDvrCamera> cameras)
		{
            try
            {
                // Return other appropriate status values if authentication fails, for example.
                // Return other appropriate status values if authentication fails, for example.
                
                
                Uri url = new Uri(connectionInfo.HostName);
                int port = (int)connectionInfo.Port;
                if (port == 0)
                    port = url.Port;
                

                string url_str = String.Format("http://{0}:{1}/ec2/getCameras?format=csv", url.Host, port);
                //string url_str = String.Format("http://{0}:{1}/ec2/getCameras?format=csv", "127.0.0.1", 7002);

                WebRequest request = HttpWebRequest.Create(url_str);

                request.Credentials = new System.Net.NetworkCredential(connectionInfo.UserId, connectionInfo.Password);
                //request.Credentials = new System.Net.NetworkCredential("admin", "123");

                WebResponse response = request.GetResponse();

                StreamReader reader = new StreamReader(response.GetResponseStream());

                string urlText = reader.ReadToEnd();

                string[] rowsChars = { "\r\n" };

                char[] elChars = { ',' };

                string[] rows = urlText.Split(rowsChars, StringSplitOptions.None);

                int numId = -1;
                int numName = -1;
                List<string> headers = ParseString(rows[0]);

                for (int i = 0; i < headers.Count; i++)
                {
                    if (headers[i] == "physicalId")
                        numId = i;
                    if (headers[i] == "name")
                        numName = i;
                };


                if (numId != -1 && numName != -1)
                {
                    for (int i = 1; i < rows.Length; i++)
                    {
                        List<string> elements = ParseString(rows[i]);
                        if (numId < elements.Count && numName < elements.Count)
                        {
                            string name = elements[numId];
                            name = name.Trim(new Char[] { '{', '}' });
                            name = name.ToUpper();
                            cameras.Add(new OemDvrCamera(name, elements[numName]));
                        }
                    }
                }


                _logger.InfoFormat("Found {0} cameras.", cameras.Count);
                return OemDvrStatus.Succeeded;
            }
			catch (Exception excp)
			{
				_logger.Error(excp);
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
		public OemDvrStatus PlayFootage(OemDvrConnection cnInfo, OemDvrFootageRequest pbInfo, Panel pbSurface)
		{
            try
            {
                //DateTime dt = new DateTime( 2014 ,5, 21 , 19 ,30 , 30);
                double tick = (pbInfo.StartTimeUtc.ToUniversalTime() - new DateTime(1970, 1, 1)).TotalMilliseconds;
                m_startTimeinMicroseconds = (long)tick * 1000;

           
                
                Uri url = new Uri(cnInfo.HostName);
                m_port = cnInfo.Port;
                if (m_port == 0)
                    m_port = (uint)url.Port;
                m_host = url.Host;
                
             
                
                //m_host = "127.0.0.1";
                //m_port = 7002;
                
                if (m_video == null)
                {
                    pbSurface.Resize += PbSurfaceResize;
                    m_video = new List<Video>();
                }
                m_panel = pbSurface;
                m_panel.Controls.Clear();

                
            
                for (int i = 0; i < pbInfo.DvrCameras.Length; i++)
                {
                    Video video = new Video();
                    video.cameraId = pbInfo.DvrCameras[i].CameraId;
                    if (pbInfo.DvrCameras.Length > 1)
                    {
                        Button dynamicbutton = new Button();
                        dynamicbutton.Visible = true;
                        dynamicbutton.Location = new Point(100 * i, 0);
                        dynamicbutton.Height = 200;
                        dynamicbutton.Width = 100;
                        dynamicbutton.Show();
                        pbSurface.Controls.Add(dynamicbutton);
                        video.window = dynamicbutton;
                    }
                    else video.window = null;                   
                    m_video.Add(video);
                };

                Resize(pbSurface.Width, pbSurface.Height);



                m_timer.Reset();
                m_timeAdvance = 0;

                      

			
				if ((pbInfo.DvrCameras == null) || (pbInfo.DvrCameras.Length <= 0))
				{
					_logger.Error("Playback request without cameras.");
					return OemDvrStatus.CameraListMissing;
				}
				if (pbInfo.DvrCameras.Length > 16)
				{
					_logger.ErrorFormat("Playback requesting {0} cameras.", pbInfo.DvrCameras.Length);
					return OemDvrStatus.NumberOfCamerasUnsupported;
				}

                Load();                               

				switch (pbInfo.PlaybackFunction)
				{
					case OemDvrPlaybackFunction.Start:
						{
                            Play();
							break;
						}
					case OemDvrPlaybackFunction.Forward:
						{
							break;
						}

					case OemDvrPlaybackFunction.Backward:
						{
							break;
						}
					case OemDvrPlaybackFunction.Pause:
						{
                            Pause();
							break;
						}
					case OemDvrPlaybackFunction.Stop:
						{
                            Stop();
							break;
						}

					default:
						{
							_logger.WarnFormat("Unsupported playback request: {0}", pbInfo.PlaybackFunction);
							break;
						}
				}

				return OemDvrStatus.Succeeded;
			}
			catch (Exception excp)
			{
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
		public OemDvrStatus ControlFootage(OemDvrPlaybackFunction pbFunction, uint speed)
		{
			try
			{       
				bool controlOk = false;
				switch (pbFunction)
				{
					case OemDvrPlaybackFunction.Pause:
                        {
                            Pause();
                            controlOk = true;
                            break;
                        }

					case OemDvrPlaybackFunction.Forward:
					case OemDvrPlaybackFunction.Backward:
						{
							float controlSpeed = Convert.ToSingle(speed) / 100F;
							if (pbFunction == OemDvrPlaybackFunction.Backward)
							{
								controlSpeed *= -1F;
							}
                            if (controlSpeed != m_playerSpeed)
                            {
                                Stop();
                                if (controlSpeed > m_playerSpeed)
                                    m_timeAdvance += 5000000;
                                else m_timeAdvance -= 5000000;
                                Load();
                                m_playerSpeed = controlSpeed;
                            }
                            Play();
							controlOk = true;
							break;
						}

					// Close down the session, such that the next actions can be closing the application or starting new session.
					case OemDvrPlaybackFunction.Stop:
						{
                            Stop();
                            m_video.Clear();
                            m_panel.Controls.Clear();
                            m_timer.Reset();
                            m_timeAdvance = 0;
                            controlOk = true;
							break;
						}

					default:
						{
							_logger.WarnFormat("Playback control {0} unsupported.", pbFunction);
							break;
						}
				}
				if (controlOk)
				{
					return OemDvrStatus.Succeeded;
				}
			}
			catch (Exception excp)
			{
				_logger.Error(excp);
			}

			return OemDvrStatus.ControlRequestFailed;
		}
        

		#endregion
        private void Load()
        {
            long allTime = m_timer.ElapsedMilliseconds*1000 + m_timeAdvance + m_startTimeinMicroseconds;

            string[] args = new string[] {
                    "-I", "dummy", "--ignore-config",
                    "--vout-filter=deinterlace", "--deinterlace-mode=blend"
                  };

            string mediaFile;
            for (int i = 0; i < m_video.Count; i++)
            {
                Video video = m_video[i];
                video.instance = MyLibVLC.LibVlc.libvlc_new(args.Length, args);
                mediaFile = String.Format("rtsp://{0}:{1}/{2}?pos={3}&codec={4}", m_host, m_port, video.cameraId, allTime, "mpeg2video");
                video.media = MyLibVLC.LibVlc.libvlc_media_new_location(video.instance, mediaFile);
                video.player = MyLibVLC.LibVlc.libvlc_media_player_new_from_media(video.media);
                if (video.window != null)
                    MyLibVLC.LibVlc.libvlc_media_player_set_hwnd(video.player, video.window.Handle);
                else MyLibVLC.LibVlc.libvlc_media_player_set_hwnd(video.player, m_panel.Handle);
                m_video[i] = video;
            }
        }
        private void Play()
        {
            for (int i = 0; i < m_video.Count; i++)
            {
                MyLibVLC.LibVlc.libvlc_media_player_play(m_video[i].player);
            }
            m_timer.Start();
        }
        private void Stop()
        {
            for (int i = 0; i < m_video.Count; i++)
            {
                MyLibVLC.LibVlc.libvlc_media_player_stop(m_video[i].player);
                MyLibVLC.LibVlc.libvlc_media_player_release(m_video[i].player);
                MyLibVLC.LibVlc.libvlc_media_release(m_video[i].media);
            }
 
        }
        private void Pause()
        {
            for (int i = 0; i < m_video.Count; i++)
            {
                MyLibVLC.LibVlc.libvlc_media_player_pause(m_video[i].player);
            }
            m_timer.Stop();
            
        }
        private void Resize(int w, int h)
        {
            if (m_video.Count > 1)
            {
                double value = (Math.Sqrt((double)m_video.Count * (double)h / (double)w));
                int numY = (int)Math.Round(value);
                int numX = (int)Math.Round(value * (double)w / ((double)h));
                while (numX * numY < m_video.Count)
                    numY++;
                while (numY > 1 && numX * (numY - 1) >= m_video.Count)
                    numY--;
                while (numX > 1 && (numX - 1) * numY >= m_video.Count)
                    numX--;

                int smallWidth = (int)((double)w / (double)numX);
                int smallHeight = (int)((double)h / (double)numY);
                int it = 0;
                for (int i = 0; i < numY; i++)
                {
                    for (int j = 0; j < numX; j++)
                    {
                        if (it < m_video.Count)
                        {
                            m_video[it].window.Location = new Point(smallWidth * j, smallHeight * i);
                            m_video[it].window.Width = smallWidth;
                            m_video[it].window.Height = smallHeight;
                            it++;
                        }
                    }
                }
            }            
        }
        private void PbSurfaceResize(object sender, EventArgs e)
        {
            var frame = sender as Panel;
            if (frame == null)
            {
                return;
            }

            short vidWidth = Convert.ToInt16(frame.Width);
            short vidHeight = Convert.ToInt16(frame.Height);
            if (m_video != null)
                Resize(vidWidth, vidHeight);
        }
	}
}
