using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
using Paxton.Net2.OemDvrInterfaces;
using log4net;

// DvrMiniDriver is a keyword, used by Paxton to lookup the entry point.
namespace nx.DvrMiniDriver {

public class OemMiniDriver: IOemDvrMiniDriver
{
	private static readonly ILog _logger =
            LogManager.GetLogger(MethodBase.GetCurrentMethod().DeclaringType);

	/// <summary>
	/// Return the string you would expect to see in the Net2 OEM supplier list, that identifies
    /// your system.
	/// </summary>
	/// <returns></returns>
	public string GetSupplierIdentity()
	{
		return AppInfo.displayProductName;
	}

	/// <summary>
	/// Verifies the host and credentials that will be stored in the Net2 database and with which
    /// requested footage will be viewed.
	/// </summary>
	/// <param name="connectionInfo">A structure identifying the host and user information.</param>
	/// <returns>OemDvrStatus value. Likely alternatives are : UnknownHost, InvalidUserIdPassword,
    /// InsufficientPriviledges</returns>
	public OemDvrStatus VerifyDvrCredentials(OemDvrConnection connectionInfo)
	{
        MessageBox.Show("VerifyDvrCredentials\n"+connectionInfo.HostName+"\n"
            +connectionInfo.Port+"\n"
            +connectionInfo.UserId+"\n"
            +connectionInfo.Password+"\n");

		try
		{
            if (DriverImplementation.testConnection(
                connectionInfo.HostName,
                connectionInfo.Port,
                connectionInfo.UserId,
                connectionInfo.Password))
            {
                return OemDvrStatus.Succeeded;
            }
            return OemDvrStatus.UnknownHost;
		}
		catch (Exception excp)
		{
            MessageBox.Show(excp.ToString());
			_logger.Error(excp);
		}

		// The more precise the implementation, the better support can be provided.
		return OemDvrStatus.InsufficientPriviledges;
	}

	/// <summary>
	/// Obtains a list of available cameras. Expect the credentials used to be the same as for VerifyDvrCredentials.
	/// </summary>
	/// <param name="connectionInfo">A structure identifying the host and user information.</param>
	/// <param name="cameras">An updatable list structure into which the camera detail can be added.</param>
	/// <returns>OemDvrStatus value. See the enumeration for detail.</returns>
	public OemDvrStatus GetListOfCameras(OemDvrConnection connectionInfo, List<OemDvrCamera> cameras)
	{
        MessageBox.Show("GetListOfCameras");

		try
		{
			// Return other appropriate status values if authentication fails, for example.

			cameras.AddRange(new[]
			                {
			                 	new OemDvrCamera("A3C6E7BF-F01C-4B26-BA12-92BB479CBA59", "Front door"),
			                 	new OemDvrCamera("BEA355D3-503D-4817-A577-1CC868CE824E", "Side entrance"),
			                 	new OemDvrCamera("01E74D2F-835B-42E1-9F02-12D4717617A4", "Warehouse"),
			                 	new OemDvrCamera("B833E694-231B-4276-A583-66D91CB3E6FC", "Reception")
			                });

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
			if ((pbInfo.DvrCameras == null) || (pbInfo.DvrCameras.Length <= 0))
			{
				_logger.Error("Playback request without cameras.");
				return OemDvrStatus.CameraListMissing;
			}

			switch (pbInfo.PlaybackFunction)
			{
				case OemDvrPlaybackFunction.Start:
					{
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
						break;
					}
				case OemDvrPlaybackFunction.Stop:
					{
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
			// Here, you need to support control requests that may come from the user, once the footage is playing.
			// This will be changes in speed and direction.
			bool controlOk = false;
			switch (pbFunction)
			{
				case OemDvrPlaybackFunction.Pause:
					{
						break;
					}

				case OemDvrPlaybackFunction.Forward:
				case OemDvrPlaybackFunction.Backward:
					{
						break;
					}

				// Close down the session, such that the next actions can be closing the application or starting new session.
				case OemDvrPlaybackFunction.Stop:
					{

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
}

} // namespace nx.DvrMiniDriver
