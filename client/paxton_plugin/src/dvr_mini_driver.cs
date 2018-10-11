using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Windows.Forms;
using Paxton.Net2.OemDvrInterfaces;
using log4net;
using log4net.Config;

// DvrMiniDriver is a keyword, used by Paxton to lookup the entry point.
namespace nx.DvrMiniDriver {

public class OemMiniDriver: IOemDvrMiniDriver
{
    private static class LogFactory
    {
        public static ILog configure()
        {
            string assemblyFile = Assembly.GetExecutingAssembly().CodeBase;
            string logFile = assemblyFile.Replace(".dll", ".xml").Replace("file:///", "");

            Type type = typeof(LogFactory);
            FileInfo configFile = new FileInfo(logFile);
            XmlConfigurator.ConfigureAndWatch(configFile);
            return LogManager.GetLogger(type);
        }
    }

    private static readonly ILog m_logger = LogFactory.configure();

    private static void handleException(Exception e)
    {
        m_logger.Error(e);
        if (e.GetType() == typeof(System.Net.WebException))
        {
            MessageBox.Show(
                "Server is not available",
                "Network Error",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        else
        {
            MessageBox.Show(
                "Unknown error has occurred",
                "System Error",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        Environment.Exit(0);
    }

    /// <summary>
    /// Return the string you would expect to see in the Net2 OEM supplier list, that identifies
    ///     your system.
    /// </summary>
    /// <returns></returns>
    public string GetSupplierIdentity()
    {
        m_logger.InfoFormat("Supplier Identity: {0}", AppInfo.displayProductName);
        return AppInfo.displayProductName;
    }

    /// <summary>
    /// Verifies the host and credentials that will be stored in the Net2 database and with which
    ///     requested footage will be viewed.
    /// </summary>
    /// <param name="connectionInfo">A structure identifying the host and user information.
    ///     </param>
    /// <returns>OemDvrStatus value. Likely alternatives are : UnknownHost, InvalidUserIdPassword,
    ///     InsufficientPriviledges</returns>
    public OemDvrStatus VerifyDvrCredentials(OemDvrConnection connectionInfo)
    {
        m_logger.Info("VerifyDvrCredentials");
        try
        {
            return PaxtonClient.testConnection(connectionInfo);
        }
        catch (Exception e)
        {
            m_logger.Error(e);
        }

        return OemDvrStatus.InsufficientPriviledges;
    }

    /// <summary>
    /// Obtains a list of available cameras. Expect the credentials used to be the same as for
    ///     VerifyDvrCredentials.
    /// </summary>
    /// <param name="connectionInfo">A structure identifying the host and user information.
    ///     </param>
    /// <param name="cameras">An updatable list structure into which the camera detail can be
    ///     added.</param>
    /// <returns>OemDvrStatus value. See the enumeration for detail.</returns>
    public OemDvrStatus GetListOfCameras(
        OemDvrConnection connectionInfo,
        List<OemDvrCamera> cameras)
    {
        m_logger.Info("GetListOfCameras");
        try
        {
            cameras.AddRange(PaxtonClient.requestCameras(connectionInfo));
            m_logger.InfoFormat("Found {0} cameras.", cameras.Count);
            return OemDvrStatus.Succeeded;
        }
        catch (Exception e)
        {
            m_logger.Error("Could not list cameras.");
            handleException(e);
        }
        return OemDvrStatus.FailedToListCameras;
    }

    /// <summary>
    /// Request the playback of footage according to the required speed, direction and cameras
    ///     using the supplied credentials on the OEM dll. The image is to be rendered on the
    ///     playback surface supplied.
    /// </summary>
    /// <param name="cnInfo">The user credentials structure.</param>
    /// <param name="pbInfo">The structure that holds the details of the footage and the
    ///     cameras required.</param>
    /// <param name="pbSurface">A reference to the panel on to which the image is to be
    ///     rendered.</param>
    /// <returns>The completion status of the request.</returns>
    public OemDvrStatus PlayFootage(
        OemDvrConnection cnInfo,
        OemDvrFootageRequest pbInfo,
        Panel pbSurface)
    {
        m_logger.Info("PlayFootage");

        try
        {
            if ((pbInfo.DvrCameras == null) || (pbInfo.DvrCameras.Length <= 0))
            {
                m_logger.Error("Playback request without cameras.");
                return OemDvrStatus.CameraListMissing;
            }
            m_logger.InfoFormat("Playback function {0} at {1}, cameras:\n {2}",
                pbInfo.PlaybackFunction,
                pbInfo.StartTimeUtc,
                String.Join("\n", pbInfo.DvrCameras.Select(x => x.CameraId + ":" + x.CameraName)));
            switch (pbInfo.PlaybackFunction)
            {
                case OemDvrPlaybackFunction.Start:
                    return PaxtonClient.playback(cnInfo, pbInfo);
                case OemDvrPlaybackFunction.Forward:
                case OemDvrPlaybackFunction.Backward:
                case OemDvrPlaybackFunction.Stop:
                case OemDvrPlaybackFunction.Pause:
                    return PaxtonClient.control(pbInfo.PlaybackFunction, pbInfo.PlaybackSpeed);
                default:
                {
                    m_logger.WarnFormat("Unsupported playback request: {0}",
                        pbInfo.PlaybackFunction);
                    break;
                }
            }
        }
        catch (Exception e)
        {
            handleException(e);
        }
        return OemDvrStatus.FootagePlaybackFailed;
    }

    /// <summary>
    /// Control an existing video currently playing back. It must have been originally
    ///     requested successfully using PlayFootage.
    /// </summary>
    /// <param name="pbFunction">The play back function required.</param>
    /// <param name="speed">The speed as an index of 100 where motion is involved.
    ///     100 is normal speed.</param>
    /// <returns>The completion status of the request.</returns>
    public OemDvrStatus ControlFootage(OemDvrPlaybackFunction pbFunction, uint speed)
    {
        m_logger.InfoFormat("ControlFootage: {0} / {1}", pbFunction, speed);
        try
        {
            switch (pbFunction)
            {
                case OemDvrPlaybackFunction.Start:
                case OemDvrPlaybackFunction.Forward:
                case OemDvrPlaybackFunction.Backward:
                case OemDvrPlaybackFunction.Stop:
                case OemDvrPlaybackFunction.Pause:
                    return PaxtonClient.control(pbFunction, speed);
                default:
                {
                    m_logger.WarnFormat("Unsupported control request: {0}", pbFunction);
                    break;
                }
            }
        }
        catch (Exception e)
        {
            m_logger.Error(e);
        }

        return OemDvrStatus.ControlRequestFailed;
    }
}

} // namespace nx.DvrMiniDriver
